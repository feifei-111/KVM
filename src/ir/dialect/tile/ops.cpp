#include "dialect/tile/ops.h"

#include <string>
#include <string_view>

namespace kvm::ir::tile::detail {

std::string FormatAxis(int axis) { return "axis=" + std::to_string(axis); }

int ParseAxis(std::string_view text) {
  // value after "axis="
  std::size_t i = 0;
  while (i < text.size() && text[i] != '=') ++i;
  if (i < text.size()) ++i;  // skip '='
  while (i < text.size() && text[i] == ' ') ++i;
  std::string_view v = text.substr(i);
  return v.empty() ? 0 : std::stoi(std::string(v));
}

// Read integer field `key` from a comma-separated "k1=v1, k2=v2" string.
// Returns 0 if absent.
int IntField(std::string_view text, std::string_view key) {
  std::size_t i = 0;
  while (i < text.size()) {
    std::size_t ks = i;
    while (i < text.size() && text[i] != '=') ++i;
    std::string_view k = text.substr(ks, i - ks);
    while (!k.empty() && k.front() == ' ') k.remove_prefix(1);
    while (!k.empty() && k.back() == ' ') k.remove_suffix(1);
    if (i < text.size()) ++i;  // '='
    std::size_t vs = i;
    while (i < text.size() && text[i] != ',') ++i;
    std::string_view v = text.substr(vs, i - vs);
    while (!v.empty() && v.front() == ' ') v.remove_prefix(1);
    while (!v.empty() && v.back() == ' ') v.remove_suffix(1);
    if (i < text.size()) ++i;  // ','
    if (k == key) return v.empty() ? 0 : std::stoi(std::string(v));
  }
  return 0;
}

}  // namespace kvm::ir::tile::detail
