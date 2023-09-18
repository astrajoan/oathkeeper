#pragma once

#include <thread>

#include "Master.h"
#include "Worker.h"

namespace mapreduce {

template <typename Impl>
void runSequential(vector<string> files, utils::IOManager* mgr) {
  vector<KvPair> results;
  for (const auto& fname : files) {
    LOG(INFO) << "Executing sequential map task: input file = " << fname;

    // open and read file content
    auto buffer = mgr->read(fname);

    // append current file kv pairs
    const auto tmp = map<Impl>(fname, buffer.str());
    results.insert(results.end(), tmp.begin(), tmp.end());
  }

  // sort kv pairs and write each unique key and its count
  std::sort(results.begin(), results.end());

  const string oname{"mr-out-sequential"};
  LOG(INFO) << "Executing sequential reduce task: output file = " << oname;

  stringstream ss;
  size_t i = 0, j = 0;
  while (i < results.size()) {
    vector<string> values;
    for (j = i; j < results.size() && results[i].key == results[j].key; ++j)
      values.push_back(results[j].value);

    string n = reduce<Impl>(results[i].key, std::move(values));
    ss << results[i].key << " " << n << "\n";
    i = j;
  }

  mgr->write(oname, ss);
}

template <typename Impl>
void runDistributed(vector<string> files,
                    int nReduce,
                    int nWorkers,
                    utils::IOManager* mgr) {
  string host{"localhost:50051"};

  vector<std::thread> threads;
  threads.emplace_back([&]() {
    Master master(host, std::move(files), nReduce);
    master.start();
  });

  for (int i = 0; i < nWorkers; ++i)
    threads.emplace_back([&]() {
      Worker<Impl> worker(host, mgr);
      worker.start();
    });

  // Both master and worker will automatically shutdown after task completed
  for (auto& t : threads) t.join();
}

}  // namespace mapreduce
