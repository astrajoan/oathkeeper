#pragma once

#include <fstream>
#include <sstream>

using std::string;
using std::stringstream;

namespace utils {

class IOManager {
 public:
  virtual ~IOManager() = default;  // proper cleanup for subclasses
  virtual stringstream read(const string& fname) = 0;
  virtual void write(const string& fname, const stringstream& ss) = 0;
};

class DiskIOManager : public IOManager {
 public:
  stringstream read(const string& fname) override {
    std::ifstream ifs(fname);
    if (!ifs) throw std::runtime_error("Failed to open: " + fname);

    stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer;
  }

  void write(const string& fname, const stringstream& ss) override {
    std::ofstream ofs(fname);
    ofs << ss.rdbuf();
  }
};

}  // namespace utils
