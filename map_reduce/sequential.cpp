#include "runner.h"

int main(int argc, char* argv[]) {
  utils::initEnv(&argc, &argv);
  if (argc < 2) LOG(FATAL) << "Please provide a list of input files";

  vector<string> files;
  for (int i = 1; i < argc; ++i) files.emplace_back(argv[i]);

  utils::DiskIOManager mgr;
  mapreduce::runSequential<mapreduce::WordCount>(std::move(files), &mgr);

  return 0;
}
