#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>

#include <agrpc/asio_grpc.hpp>
#include <atomic>
#include <boost/asio.hpp>
#include <memory>
#include <queue>

#include "map_reduce.grpc.pb.h"

using std::string;
using std::vector;
namespace asio = boost::asio;

DECLARE_int32(task_wait_ms);

namespace mapreduce {

class Master {
 public:
  enum class Phase { MAP, REDUCE, DONE };

  Master(const string& host, vector<string> files, int nReduce) :
      host_(host),
      files_(std::move(files)),
      nMap_(files_.size()),
      nReduce_(nReduce > 0 ? nReduce : files_.size()) {}

  Master(const Master&) = delete;
  Master& operator=(const Master&) = delete;

  void start() {
    if (!stopped_.exchange(false)) return;

    grpc::ServerBuilder builder;
    context_ =
        std::make_unique<agrpc::GrpcContext>(builder.AddCompletionQueue());
    builder.AddListeningPort(host_, grpc::InsecureServerCredentials());
    builder.RegisterService(&rpcService_);
    server_ = builder.BuildAndStart();

    registerRequestTaskHandler();
    registerNotifyDoneHandler();
    prepareMap();

    context_->run();
    stop();
  }

  void stop() {
    if (stopped_.exchange(true)) return;

    server_->Shutdown();
    server_.reset();
    context_->stop();
    context_.reset();
  }

 private:
  void prepareMap();
  void prepareReduce();
  void prepareDone();

  void registerRequestTaskHandler();
  void registerNotifyDoneHandler();

  asio::awaitable<void> handleRequestTask(
      google::protobuf::Empty& request,
      grpc::ServerAsyncResponseWriter<proto::TaskResponse>& writer);
  asio::awaitable<void> handleNotifyDone(
      proto::NotifyInfo& request,
      grpc::ServerAsyncResponseWriter<google::protobuf::Empty>& writer);
  asio::awaitable<void> sendTaskResponse(
      grpc::ServerAsyncResponseWriter<proto::TaskResponse>& writer);
  asio::awaitable<void> recordTaskComplete(
      proto::NotifyInfo& request,
      grpc::ServerAsyncResponseWriter<google::protobuf::Empty>& writer);

  const string host_;
  const vector<string> files_;
  const int nMap_;
  const int nReduce_;

  std::unique_ptr<agrpc::GrpcContext> context_;
  std::unique_ptr<grpc::Server> server_;
  proto::MasterService::AsyncService rpcService_;

  std::atomic<bool> stopped_{true};

  /*
   * the way master manages its states
   * token_: the current most up to date token to be assigned for the next
             requested worker
   * phase_: the current phase of the server, could be one of MAP, REDUCE, DONE
   * todo_ : a FIFO queue to hold all the tasks to-be-assigned in the form of
   *         TaskResponse
   * curr_ : a hash map that records tasks that are currently being processed in
   *         the form of {token -> mapId / reduceId}
   * done_ : a hash set that hold all completed tasks for the current phase in
   *         the form of their IDs
   */
  int token_{0};
  Phase phase_;
  std::queue<proto::TaskResponse> todo_;
  std::unordered_map<int, int> curr_;
  std::unordered_set<int> done_;
};

}  // namespace mapreduce
