#include "runner.h"

DEFINE_int32(n_reduce, 0, "Number of reduce tasks, default to same as nMap");
DEFINE_int32(n_workers, 3, "Number of workers to run in parallel");

int main(int argc, char* argv[]) {
  utils::initEnv(&argc, &argv);
  if (argc < 2) LOG(FATAL) << "Please provide a list of input files";

  vector<string> files;
  for (int i = 1; i < argc; ++i) files.emplace_back(argv[i]);

  mapreduce::runDistributed<mapreduce::WordCount>(
      std::move(files), FLAGS_n_reduce, FLAGS_n_workers, nullptr);

  return 0;
}
