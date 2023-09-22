#include "Master.h"

#include <boost/asio/experimental/awaitable_operators.hpp>

#include "utils.h"

DEFINE_int32(task_wait_ms, 5000, "Milliseconds to wait for worker tasks");

namespace mapreduce {

void Master::prepareMap() {
  LOG(INFO) << "Preparing map phase data";

  CHECK(todo_.empty());
  CHECK(curr_.empty());
  done_.clear();

  for (int i = 0; i < nMap_; ++i) {
    proto::TaskResponse response;
    response.set_state(proto::State::MAP);
    response.set_token(token_++);
    response.mutable_map_task()->set_map_id(i);
    response.mutable_map_task()->set_n_reduce(nReduce_);
    response.mutable_map_task()->set_fname(files_[i]);
    todo_.push(std::move(response));
  }
  phase_ = Phase::MAP;

  LOG(INFO) << "Map phase started";
}

void Master::prepareReduce() {
  LOG(INFO) << "Preparing reduce phase data";

  CHECK(todo_.empty());
  CHECK(curr_.empty());
  done_.clear();

  for (int i = 0; i < nReduce_; ++i) {
    proto::TaskResponse response;
    response.set_state(proto::State::REDUCE);
    response.set_token(token_++);
    response.mutable_reduce_task()->set_reduce_id(i);
    response.mutable_reduce_task()->set_n_map(nMap_);
    todo_.push(std::move(response));
  }
  phase_ = Phase::REDUCE;

  LOG(INFO) << "Reduce phase started";
}

void Master::prepareDone() {
  CHECK(todo_.empty());
  CHECK(curr_.empty());
  phase_ = Phase::DONE;

  asio::co_spawn(
      *context_,
      [this]() -> asio::awaitable<void> {
        co_await utils::asyncSleepMs(FLAGS_task_wait_ms);
        server_->Shutdown();
      },
      asio::detached);

  LOG(INFO) << "Done phase, master stopping in " << FLAGS_task_wait_ms << "ms";
}

void Master::registerRequestTaskHandler() {
  LOG(INFO) << "Registering and running RequestTask handler";

  agrpc::repeatedly_request(
      &proto::MasterService::AsyncService::RequestRequestTask,
      rpcService_,
      asio::bind_executor(
          *context_,
          [this](auto&, auto& request, auto& writer) -> asio::awaitable<void> {
            co_await handleRequestTask(request, writer);
          }));
}

void Master::registerNotifyDoneHandler() {
  LOG(INFO) << "Registering and running NotifyDone handler";

  agrpc::repeatedly_request(
      &proto::MasterService::AsyncService::RequestNotifyDone,
      rpcService_,
      asio::bind_executor(
          *context_,
          [this](auto&, auto& request, auto& writer) -> asio::awaitable<void> {
            co_await handleNotifyDone(request, writer);
          }));
}

asio::awaitable<void> Master::handleRequestTask(
    google::protobuf::Empty& request,
    grpc::ServerAsyncResponseWriter<proto::TaskResponse>& writer) {
  switch (phase_) {
    case Phase::MAP:
    case Phase::REDUCE:
      co_await sendTaskResponse(writer);
      break;
    case Phase::DONE: {
      proto::TaskResponse response;
      response.set_state(proto::State::DONE);
      co_await agrpc::finish(writer, response, grpc::Status::OK);
      break;
    }
    default:
      LOG(FATAL) << "Wrong state: not supposed to be here!";
      break;
  }
}

asio::awaitable<void> Master::handleNotifyDone(
    proto::NotifyInfo& request,
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty>& writer) {
  switch (phase_) {
    case Phase::MAP:
    case Phase::REDUCE:
      co_await recordTaskComplete(request, writer);
      break;
    case Phase::DONE: {
      google::protobuf::Empty empty;
      co_await agrpc::finish(writer, empty, grpc::Status::OK);
      break;
    }
    default:
      LOG(FATAL) << "Wrong state: not supposed to be here!";
      break;
  }
}

asio::awaitable<void> Master::sendTaskResponse(
    grpc::ServerAsyncResponseWriter<proto::TaskResponse>& writer) {
  proto::TaskResponse response;
  if (todo_.empty()) {
    response.set_state(proto::State::WAIT);
    co_await agrpc::finish(writer, response, grpc::Status::OK);
    co_return;
  }

  response = std::move(todo_.front());
  todo_.pop();

  const auto token = response.token();
  const auto id = phase_ == Phase::MAP ? response.map_task().map_id()
                                       : response.reduce_task().reduce_id();
  const auto msg = phase_ == Phase::MAP ? ", map id = " : ", reduce id = ";
  curr_[token] = id;

  LOG(INFO) << "Sending task to worker: token = " << token << msg << id;

  using namespace asio::experimental::awaitable_operators;
  co_await (utils::asyncSleepMs(FLAGS_task_wait_ms) &&
            agrpc::finish(writer, response, grpc::Status::OK));

  if (done_.find(id) == done_.end()) {
    LOG(INFO) << "Found unresponsive task: token = " << token << msg << id;

    response.set_token(token_++);
    curr_.erase(token);
    todo_.push(response);
  }
}

asio::awaitable<void> Master::recordTaskComplete(
    proto::NotifyInfo& request,
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty>& writer) {
  const auto token = request.token();

  google::protobuf::Empty empty;
  if (curr_.find(token) == curr_.end()) {
    LOG(WARNING) << "Ignored invalid task completion: token = " << token;
    co_await agrpc::finish(writer, empty, grpc::Status::OK);
    co_return;
  }

  const auto id = curr_[token];
  const auto msg = phase_ == Phase::MAP ? ", map id = " : ", reduce id = ";
  curr_.erase(token);
  done_.insert(id);

  LOG(INFO) << "Recording task complete: token = " << token << msg << id;

  if (todo_.size() + curr_.size() == 0) {
    if (phase_ == Phase::MAP && done_.size() == nMap_) prepareReduce();
    else if (phase_ == Phase::MAP)
      LOG(FATAL) << "Map phase ended without finishing all map tasks!";
    else if (phase_ == Phase::REDUCE && done_.size() == nReduce_)
      prepareDone();
    else
      LOG(FATAL) << "Reduce phase ended without finishing all reduce tasks!";
  }

  co_await agrpc::finish(writer, empty, grpc::Status::OK);
}

}  // namespace mapreduce
