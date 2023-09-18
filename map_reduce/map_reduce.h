#pragma once

#include <boost/algorithm/string/join.hpp>
#include <string>
#include <unordered_set>
#include <vector>

using std::string;
using std::vector;

namespace mapreduce {

struct KvPair {
  string key;
  string value;
  KvPair(const string& k, const string& v) : key(k), value(v) {}
  bool operator<(const KvPair& p) const { return key < p.key; }
};

template <typename Impl>
vector<KvPair> map(const string& fname, const string& content) {
  return Impl::map(fname, content);
}

template <typename Impl>
string reduce(const string& key, vector<string> values) {
  return Impl::reduce(key, values);
}

struct WordCount {
  static vector<KvPair> map(const string& fname, const string& content) {
    // parse the content into word list
    vector<string> words;
    string s;
    for (const auto& c : content) {
      if (!isalpha(c) && !s.empty()) {
        words.push_back(s);
        s.clear();
      } else if (isalpha(c))
        s.push_back(std::tolower(c));
    }

    // save each word as a kv pair
    vector<KvPair> vkvp;
    for (const auto& w : words) vkvp.emplace_back(w, "1");
    return vkvp;
  }

  static string reduce(const string& key, vector<string>& values) {
    return std::to_string(values.size());
  }
};

struct Indexer {
  static vector<KvPair> map(const string& fname, const string& content) {
    // parse the content into word list
    std::unordered_set<string> words;
    string s;
    for (const auto& c : content) {
      if (!isalpha(c) && !s.empty()) {
        words.insert(s);
        s.clear();
      } else if (isalpha(c))
        s.push_back(std::tolower(c));
    }

    vector<KvPair> vkvp;
    for (const auto& w : words) vkvp.emplace_back(w, fname);
    return vkvp;
  }

  static string reduce(const string& key, vector<string>& values) {
    std::sort(values.begin(), values.end());
    const auto res = boost::algorithm::join(values, ",");
    return std::to_string(values.size()) + " " + res;
  }
};

}  // namespace mapreduce
