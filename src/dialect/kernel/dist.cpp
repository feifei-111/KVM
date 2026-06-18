#include "dialect/kernel/dist.h"

#include <string>
#include <string_view>

namespace kvm::ir::kernel::detail {
namespace {

std::string_view Field(std::string_view text, std::string_view key) {
  std::size_t i = 0;
  while (i < text.size()) {
    std::size_t ks = i;
    while (i < text.size() && text[i] != '=') ++i;
    std::string_view k = text.substr(ks, i - ks);
    while (!k.empty() && k.front() == ' ') k.remove_prefix(1);
    while (!k.empty() && k.back() == ' ') k.remove_suffix(1);
    if (i < text.size()) ++i;  // '='
    std::size_t vs = i;
    int depth = 0;
    for (; i < text.size(); ++i) {
      char c = text[i];
      if (c == '[')
        ++depth;
      else if (c == ']')
        --depth;
      else if (c == ',' && depth == 0)
        break;
    }
    std::string_view v = text.substr(vs, i - vs);
    while (!v.empty() && v.front() == ' ') v.remove_prefix(1);
    while (!v.empty() && v.back() == ' ') v.remove_suffix(1);
    if (i < text.size()) ++i;  // ','
    if (k == key) return v;
  }
  return {};
}

}  // namespace

std::string FormatFromTo(int from, int to) {
  return "from=" + std::to_string(from) + ", to=" + std::to_string(to);
}

void ParseFromTo(std::string_view text, int& from, int& to) {
  std::string_view f = Field(text, "from");
  std::string_view t = Field(text, "to");
  from = f.empty() ? 0 : std::stoi(std::string(f));
  to = t.empty() ? 0 : std::stoi(std::string(t));
}

std::string FormatRanks(const std::vector<int>& ranks) {
  return "ranks=" + text::FormatInts(ranks);
}

std::vector<int> ParseRanks(std::string_view text) {
  return text::ParseInts(Field(text, "ranks"));
}

std::string FormatBroadcast(int from, const std::vector<int>& to) {
  return "from=" + std::to_string(from) + ", to=" + text::FormatInts(to);
}

void ParseBroadcast(std::string_view text, int& from, std::vector<int>& to) {
  std::string_view f = Field(text, "from");
  from = f.empty() ? 0 : std::stoi(std::string(f));
  to = text::ParseInts(Field(text, "to"));
}

}  // namespace kvm::ir::kernel::detail
