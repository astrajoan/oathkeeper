#include <gtest/gtest.h>

#include <filesystem>
#include <set>

#include "runner.h"
#include "test_utils.h"

namespace {

// clang-format off
static const vector<string> kDiskFiles{
  "pg-being_ernest.txt",
  "pg-dorian_gray.txt",
  "pg-frankenstein.txt",
  "pg-grimm.txt",
  "pg-huckleberry_finn.txt",
  "pg-metamorphosis.txt",
  "pg-sherlock_holmes.txt",
  "pg-tom_sawyer.txt"
};
// clang-format on

}  // namespace

TEST(MrImplTests, simpleWordCountTest) {
  FLAGS_task_wait_ms = 1000;  // 1 second should suffice for testing
  int nMap = 8, nReduce = 5, nWorkers = 3;

  vector<string> tmpFiles;
  for (int i = 0; i < nMap; ++i)
    tmpFiles.emplace_back(std::format("file{}", i));

  utils::MemoryIOManager mgr;
  mapreduce::runSequential<mapreduce::WordCount>(tmpFiles, &mgr);
  mapreduce::runDistributed<mapreduce::WordCount>(
      tmpFiles, nReduce, nWorkers, &mgr);

  utils::compareResult(&mgr, nReduce);
}

TEST(MrImplTests, realWordCountTest) {
  FLAGS_task_wait_ms = 1000;  // 1 second should suffice for testing
  int nReduce = 20, nWorkers = 5;

  for (const auto& fname : kDiskFiles)
    EXPECT_TRUE(std::filesystem::exists(fname))
        << "Cannot find input files, please check your cmake outputs!";

  utils::MemoryIOManager mgr;
  mapreduce::runSequential<mapreduce::WordCount>(kDiskFiles, &mgr);
  mapreduce::runDistributed<mapreduce::WordCount>(
      kDiskFiles, nReduce, nWorkers, &mgr);

  utils::compareResult(&mgr, nReduce);
}

TEST(MrImplTests, simpleIndexerTest) {
  FLAGS_task_wait_ms = 1000;  // 1 second should suffice for testing
  int nMap = 8, nReduce = 5, nWorkers = 3;

  vector<string> tmpFiles;
  for (int i = 0; i < nMap; ++i)
    tmpFiles.emplace_back(std::format("file{}", i));

  utils::MemoryIOManager mgr;
  mapreduce::runSequential<mapreduce::Indexer>(tmpFiles, &mgr);
  mapreduce::runDistributed<mapreduce::Indexer>(
      tmpFiles, nReduce, nWorkers, &mgr);

  utils::compareResult(&mgr, nReduce);
}

TEST(MrImplTests, realIndexerTest) {
  FLAGS_task_wait_ms = 1000;  // 1 second should suffice for testing
  int nReduce = 20, nWorkers = 5;

  for (const auto& fname : kDiskFiles)
    EXPECT_TRUE(std::filesystem::exists(fname))
        << "Cannot find input files, please check your cmake outputs!";

  utils::MemoryIOManager mgr;
  mapreduce::runSequential<mapreduce::Indexer>(kDiskFiles, &mgr);
  mapreduce::runDistributed<mapreduce::Indexer>(
      kDiskFiles, nReduce, nWorkers, &mgr);

  utils::compareResult(&mgr, nReduce);
}
