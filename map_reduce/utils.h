#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <boost/asio.hpp>
#include <chrono>

using std::chrono::milliseconds;
namespace asio = boost::asio;

namespace utils {

inline asio::awaitable<void> asyncSleepMs(int ms) {
  auto ctx = co_await asio::this_coro::executor;
  asio::steady_timer timer(ctx, milliseconds(ms));
  co_await timer.async_wait(asio::use_awaitable);
}

inline void initEnv(int* argc, char*** argv) {
  gflags::ParseCommandLineFlags(argc, argv, true);
  // Ensure log outputs are written to the console for more visible debugging
  FLAGS_stderrthreshold = google::GLOG_INFO;
  google::InitGoogleLogging((*argv)[0]);
}

}  // namespace utils
