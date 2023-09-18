#include "Worker.h"

DEFINE_string(host, "localhost", "Master hostname or IP to connect to");
DEFINE_int32(port, 50051, "Master port to connect to");

int main(int argc, char* argv[]) {
  utils::initEnv(&argc, &argv);

  const auto host = std::format("{}:{}", FLAGS_host, FLAGS_port);
  mapreduce::Worker<mapreduce::WordCount> worker(host, nullptr);
  worker.start();

  return 0;
}
