#pragma once

// Small text helpers shared by the dialects' impl codecs: formatting and
// parsing the compact `key=value` / list forms used inside `<...>`.
//
// These are dialect-local conveniences (kept at the dialect root so every
// dialect shares one copy); the serialization framework only requires each impl
// to expose Serialize()/Deserialize(), and these helpers keep those short.

#include <string>
#include <string_view>
#include <vector>

namespace kvm::ir::dialect::text {

// "[1,2,3]" <-> vector<int>. Empty vector -> "[]".
inline std::string FormatInts(const std::vector<int>& v) {
  std::string out = "[";
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (i) out += ",";
    out += std::to_string(v[i]);
  }
  out += "]";
  return out;
}

// Parse "[1,2,3]" (surrounding brackets required) into ints.
inline std::vector<int> ParseInts(std::string_view s) {
  std::vector<int> out;
  std::size_t i = 0;
  while (i < s.size() && s[i] != '[') ++i;
  if (i < s.size()) ++i;  // skip '['
  std::string cur;
  for (; i < s.size() && s[i] != ']'; ++i) {
    char c = s[i];
    if (c == ',') {
      if (!cur.empty()) {
        out.push_back(std::stoi(cur));
        cur.clear();
      }
    } else if (c != ' ') {
      cur += c;
    }
  }
  if (!cur.empty()) out.push_back(std::stoi(cur));
  return out;
}

// "[[3],[7,8]]" <-> vector<vector<int>>.
inline std::string FormatIntLists(const std::vector<std::vector<int>>& v) {
  std::string out = "[";
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (i) out += ",";
    out += FormatInts(v[i]);
  }
  out += "]";
  return out;
}

inline std::vector<std::vector<int>> ParseIntLists(std::string_view s) {
  std::vector<std::vector<int>> out;
  // find each balanced inner [...] after the outer '['
  std::size_t i = 0;
  while (i < s.size() && s[i] != '[') ++i;
  if (i < s.size()) ++i;  // skip outer '['
  for (; i < s.size() && s[i] != ']';) {
    if (s[i] == '[') {
      std::size_t start = i;
      int depth = 0;
      for (; i < s.size(); ++i) {
        if (s[i] == '[')
          ++depth;
        else if (s[i] == ']') {
          --depth;
          if (depth == 0) {
            ++i;
            break;
          }
        }
      }
      out.push_back(ParseInts(s.substr(start, i - start)));
    } else {
      ++i;  // skip commas / spaces
    }
  }
  return out;
}

}  // namespace kvm::ir::dialect::text
