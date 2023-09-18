#pragma once

#include <grpcpp/grpcpp.h>

#include <agrpc/asio_grpc.hpp>
#include <atomic>
#include <format>
#include <memory>

#include "IOManager.h"
#include "map_reduce.grpc.pb.h"
#include "map_reduce.h"
#include "utils.h"

namespace mapreduce {

template <typename Impl>
class Worker {
 public:
  Worker(const string& host, utils::IOManager* mgr) :
      stub_(grpc::CreateChannel(host, grpc::InsecureChannelCredentials())),
      iomgr_(mgr == nullptr ? getIOManager() : mgr) {}

  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  void start() {
    if (!stopped_.exchange(false)) return;

    context_ = std::make_unique<agrpc::GrpcContext>();
    asio::co_spawn(*context_, workerLoopWrapper(), asio::detached);
    context_->run();
    stop();
  }

  void stop() {
    if (stopped_.exchange(true)) return;

    context_->stop();
    context_.reset();
  }

 private:
  static utils::IOManager* getIOManager() {
    static utils::DiskIOManager instance;
    return &instance;
  }

  static constexpr int kMaxRetries = 3;
  static constexpr int kWorkerWaitMs = 500;

  asio::awaitable<void> workerLoopWrapper();
  asio::awaitable<void> workerLoop();
  asio::awaitable<proto::TaskResponse> callRequestTask();
  asio::awaitable<void> callNotifyDone(int token);

  // These do not need to be coroutines since they do not involve async I/O
  void doMapTask(const proto::MapInfo& info);
  void doReduceTask(const proto::ReduceInfo& info);

  proto::MasterService::Stub stub_;
  std::unique_ptr<agrpc::GrpcContext> context_;
  utils::IOManager* iomgr_;

  std::atomic<bool> stopped_{true};
  int failCnt_{0};
};

template <typename Impl>
asio::awaitable<void> Worker<Impl>::workerLoopWrapper() {
  try {
    co_await workerLoop();
  } catch (const std::exception& ex) {
    LOG(ERROR) << "Worker throws an exception: " << ex.what();
  }
}

template <typename Impl>
asio::awaitable<void> Worker<Impl>::workerLoop() {
  LOG(INFO) << "Worker started";

  while (!stopped_) {
    const auto response = co_await callRequestTask();
    switch (response.state()) {
      case proto::State::WAIT:
        LOG(INFO) << "Sleeping for " << kWorkerWaitMs << "ms since master "
                  << "responded to wait, or master is temporarily down";
        co_await utils::asyncSleepMs(kWorkerWaitMs);
        break;
      case proto::State::DONE:
        if (!failCnt_) LOG(INFO) << "Master responded done, stopping worker";
        stopped_.exchange(true);
        break;
      case proto::State::MAP:
        doMapTask(response.map_task());
        co_await callNotifyDone(response.token());
        break;
      case proto::State::REDUCE:
        doReduceTask(response.reduce_task());
        co_await callNotifyDone(response.token());
        break;
      default:
        LOG(FATAL) << "Wrong state: not supposed to be here!";
        break;
    }
  }

  LOG(INFO) << "Worker stopped";
}

template <typename Impl>
asio::awaitable<proto::TaskResponse> Worker<Impl>::callRequestTask() {
  using RPC =
      agrpc::ClientRPC<&proto::MasterService::Stub::PrepareAsyncRequestTask>;

  grpc::ClientContext client_context;
  google::protobuf::Empty empty;
  proto::TaskResponse response;

  const auto status = co_await RPC::request(
      *context_, stub_, client_context, empty, response, asio::use_awaitable);

  if (status.ok()) {
    // clear failed RPC count if the master has responded back
    failCnt_ = 0;
  } else if (++failCnt_ >= kMaxRetries) {
    response.set_state(proto::State::DONE);
    LOG(ERROR) << "Master has failed to dispatch a task " << kMaxRetries
               << " times in a row, shutting down worker";
  } else {
    response.set_state(proto::State::WAIT);
    LOG(ERROR) << "Error RPC status in callRequestTask()!";
  }

  co_return response;
}

template <typename Impl>
asio::awaitable<void> Worker<Impl>::callNotifyDone(int token) {
  using RPC =
      agrpc::ClientRPC<&proto::MasterService::Stub::PrepareAsyncNotifyDone>;

  grpc::ClientContext client_context;
  proto::NotifyInfo info;
  info.set_token(token);
  google::protobuf::Empty empty;

  const auto status = co_await RPC::request(
      *context_, stub_, client_context, info, empty, asio::use_awaitable);

  // error handling relies on callRequestTask
  if (!status.ok()) LOG(ERROR) << "Error RPC status in callNotifyDone()!";

  LOG(INFO) << "Reported task " << token << " as finished";
}

template <typename Impl>
void Worker<Impl>::doMapTask(const proto::MapInfo& info) {
  const auto& fname = info.fname();
  const auto mapId = info.map_id();
  const auto nReduce = info.n_reduce();

  LOG(INFO) << "Executing map task: input file = " << fname
            << ", mapId = " << mapId << ", nReduce = " << nReduce;

  // read file content
  auto buffer = iomgr_->read(fname);
  const auto words = map<Impl>(fname, buffer.str());  // call map method

  // write key value pairs to corresponding output file
  vector<stringstream> vss(nReduce);

  for (const auto& w : words) {
    auto reduceId = std::hash<std::string>{}(w.key) % nReduce;
    vss[reduceId] << w.key << " " << w.value << "\n";
  }

  for (int i = 0; i < nReduce; ++i)
    iomgr_->write(std::format("mr-{}-{}", mapId, i), vss[i]);
}

template <typename Impl>
void Worker<Impl>::doReduceTask(const proto::ReduceInfo& info) {
  const auto reduceId = info.reduce_id();
  const auto nMap = info.n_map();
  const auto fname = std::format("mr-out-{}", reduceId);

  LOG(INFO) << "Executing reduce task: output file = " << fname
            << ", reduceId = " << reduceId << ", nMap = " << nMap;

  // store all kv pairs within current reduceId
  string key, value;
  vector<KvPair> vkvp;
  for (size_t i = 0; i < nMap; ++i) {
    auto ss = iomgr_->read(std::format("mr-{}-{}", i, reduceId));
    while (ss >> key >> value) vkvp.emplace_back(key, value);
  }

  // sort kv pairs and write each unique key and its count
  std::sort(vkvp.begin(), vkvp.end());

  stringstream ss;
  size_t i = 0, j = 0;
  while (i < vkvp.size()) {
    vector<string> values;
    for (j = i; j < vkvp.size() && vkvp[i].key == vkvp[j].key; ++j)
      values.push_back(vkvp[j].value);

    const auto res =
        reduce<Impl>(vkvp[i].key, std::move(values));  // call reduce method
    ss << vkvp[i].key << " " << res << "\n";
    i = j;
  }

  iomgr_->write(fname, ss);
}

}  // namespace mapreduce
