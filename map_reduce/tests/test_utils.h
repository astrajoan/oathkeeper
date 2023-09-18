#pragma once

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <mutex>
#include <set>
#include <unordered_map>

#include "IOManager.h"

namespace utils {

class MemoryIOManager : public DiskIOManager {
 public:
  stringstream read(const string& fname) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (files_.find(fname) != files_.end())
        return stringstream(files_[fname]);
    }
    return DiskIOManager::read(fname);
  }

  void write(const string& fname, const stringstream& ss) override {
    std::lock_guard<std::mutex> lock(mutex_);
    files_[fname] = ss.str();
  }

  bool existsInMemory(const string& fname) {
    std::lock_guard<std::mutex> lock(mutex_);
    return files_.find(fname) != files_.end();
  }

 protected:
  // clang-format off
  std::unordered_map<string, string> files_{
    {"file0", "aaa bbb bbb ccc ccc ccc"},
    {"file1", "aaa aab abb bbb"},
    {"file2", "aac aca caa aac aca caa"},
    {"file3", "bcc bcc bcc bcc bcc bcc bcc bcc bcc"},
    {"file4", "bbb ccc bbb ccc aca aac"},
    {"file5", "abc"},
    {"file6", "aaa ccc bbb cca abc aca acb cba ccb bac bcc"},
    {"file7", "bbc aab abb cba aca"}
  };
  // clang-format on

  std::mutex mutex_;
};

inline void compareResult(MemoryIOManager* mgr, int nReduce) {
  std::set<string> sequential;
  const string seqName{"mr-out-sequential"};
  EXPECT_TRUE(mgr->existsInMemory(seqName));
  auto ss = mgr->read(seqName);

  string line;
  while (std::getline(ss, line)) {
    EXPECT_EQ(sequential.find(line), sequential.end());
    sequential.insert(line);
  }

  std::set<string> distributed;
  for (int i = 0; i < nReduce; ++i) {
    const auto fname = std::format("mr-out-{}", i);
    EXPECT_TRUE(mgr->existsInMemory(fname));
    auto ss = mgr->read(fname);

    while (std::getline(ss, line)) {
      EXPECT_EQ(distributed.find(line), distributed.end());
      distributed.insert(line);
    }
  }
  EXPECT_EQ(distributed, sequential);
}

}  // namespace utils
