#include <format>

#include "Master.h"
#include "utils.h"

DEFINE_string(host, "localhost", "Master hostname or IP to listen on");
DEFINE_int32(port, 50051, "Master port to listen on");
DEFINE_int32(n_reduce, 0, "Number of reduce tasks, default to same as nMap");

int main(int argc, char* argv[]) {
  utils::initEnv(&argc, &argv);
  if (argc < 2) LOG(FATAL) << "Please provide a list of input files";

  vector<string> files;
  for (int i = 1; i < argc; ++i) files.emplace_back(argv[i]);

  const auto host = std::format("{}:{}", FLAGS_host, FLAGS_port);
  mapreduce::Master master(host, std::move(files), FLAGS_n_reduce);
  master.start();

  return 0;
}
