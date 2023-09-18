#include <format>

#include "Master.h"
#include "utils.h"

DEFINE_string(host, "localhost", "Master hostname or IP to listen on");
DEFINE_int32(port, 50051, "Master port to listen on");
DEFINE_int32(n_reduce, 0, "Number of reduce tasks, default to same as nMap");

int main(int argc, char* argv[]) {
  // TQ: could we leverage gflags::SetUsageMessage to provide a help message to
  // our users? the mixture of positional arguments and flags may cause some
  // confusion here; you could research how other UNIX CLI tools handle this ✅
  // TQ: the output of gflags help message seems messy by itself, let's provide
  // the help message in repo README instead of here ❌
  utils::initEnv(&argc, &argv);
  if (argc < 2) LOG(FATAL) << "Please provide a list of input files";

  vector<string> files;
  for (int i = 1; i < argc; ++i) files.emplace_back(argv[i]);

  const auto host = std::format("{}:{}", FLAGS_host, FLAGS_port);
  mapreduce::Master master(host, std::move(files), FLAGS_n_reduce);
  master.start();

  return 0;
}
