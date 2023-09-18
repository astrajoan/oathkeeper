#include <gtest/gtest.h>

#include <set>

#include "runner.h"
#include "test_utils.h"

namespace mapreduce {

struct NoCrash {
  static vector<KvPair> map(const string& fname, const string& content) {
    return {{"abc", fname},
            {"jkl", std::to_string(fname.size())},
            {"qqq", std::to_string(content.size())},
            {"xzz", "xyzzy"}};
  }

  static string reduce(const string& key, vector<string>& values) {
    std::sort(values.begin(), values.end());
    return boost::algorithm::join(values, ",");
  }
};

struct Crash : public NoCrash {
  static void maybeCrash() {
    int prob = std::rand() % 1000;
    if (prob < 333) {
      throw std::runtime_error("Intentionally crashing");
    } else if (prob < 666) {
      int ms = 500 + std::rand() % 1000;
      std::this_thread::sleep_for(milliseconds(ms));
    }
  }

  static vector<KvPair> map(const string& fname, const string& content) {
    maybeCrash();
    return NoCrash::map(fname, content);
  }

  static string reduce(const string& key, vector<string>& values) {
    maybeCrash();
    return NoCrash::reduce(key, values);
  }
};

struct EarlyExit : public NoCrash {
  static string reduce(const string& key, vector<string> values) {
    // some reduce tasks sleep for a long time, potentially seeing if a worker
    // accidentally exits early
    if (key.find("a") != string::npos || key.find("z") != string::npos)
      std::this_thread::sleep_for(milliseconds(1500));
    return NoCrash::reduce(key, values);
  }
};

}  // namespace mapreduce

TEST(CrashTests, randomCrashTest) {
  FLAGS_task_wait_ms = 1000;  // 1 second should suffice for testing
  int nMap = 8, nReduce = 10, nWorkers = 5;

  vector<string> tmpFiles;
  for (int i = 0; i < nMap; ++i)
    tmpFiles.emplace_back(std::format("file{}", i));

  utils::MemoryIOManager mgr;
  mapreduce::runSequential<mapreduce::NoCrash>(tmpFiles, &mgr);

  string host{"localhost:50051"};

  // first, start the master in its own thread
  std::thread masterThread([&]() {
    mapreduce::Master master(host, std::move(tmpFiles), nReduce);
    master.start();
  });

  vector<string> reduceFiles;
  for (int i = 0; i < nReduce; ++i)
    reduceFiles.emplace_back(std::format("mr-out-{}", i));

  std::unordered_set<string> done;
  while (done.size() < nReduce) {
    // while tasks not completed, run a group of workers until they all crash
    LOG(INFO) << "Running " << nWorkers << " new workers";

    vector<std::thread> workerThreads;
    for (int i = 0; i < nWorkers; ++i)
      workerThreads.emplace_back([&]() {
        mapreduce::Worker<mapreduce::Crash> worker(host, &mgr);
        worker.start();
      });

    for (auto& t : workerThreads) t.join();

    // check which files have been completed, insert into set if needed
    for (const auto& fname : reduceFiles)
      if (mgr.existsInMemory(fname)) done.insert(fname);
  }

  // while loop has finished, the master thread can finally join
  masterThread.join();

  utils::compareResult(&mgr, nReduce);
}

TEST(CrashTest, earlyExitTest) {
  FLAGS_task_wait_ms = 2000;  // 2 seconds to test early exit behavior
  int nMap = 8, nReduce = 10, nWorkers = 5;

  vector<string> tmpFiles;
  for (int i = 0; i < nMap; ++i)
    tmpFiles.emplace_back(std::format("file{}", i));

  utils::MemoryIOManager mgr;
  mapreduce::runSequential<mapreduce::NoCrash>(tmpFiles, &mgr);

  string host{"localhost:50051"};

  std::mutex mtx;
  std::condition_variable cv;
  bool threadExit = false;

  // first, start the master in its own thread
  vector<std::thread> threads;
  threads.emplace_back([&]() {
    mapreduce::Master master(host, std::move(tmpFiles), nReduce);
    master.start();
    // change exit flag (lock protected) and notify CV (not protected)
    {
      std::lock_guard<std::mutex> lock(mtx);
      threadExit = true;
    }
    cv.notify_one();
  });

  for (int i = 0; i < nWorkers; ++i)
    threads.emplace_back([&]() {
      mapreduce::Worker<mapreduce::EarlyExit> worker(host, &mgr);
      worker.start();
      // change exit flag (lock protected) and notify CV (not protected)
      {
        std::lock_guard<std::mutex> lock(mtx);
        threadExit = true;
      }
      cv.notify_one();
    });

  // no logic to protect, but CV needs to be used in conjunction with mutex
  {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]() { return threadExit; });
  }

  utils::compareResult(&mgr, nReduce);

  // finally, allow all threads to finish after comparing the results
  for (auto& t : threads) t.join();
}
