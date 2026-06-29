#include "dialect/task/ops.h"

#include <string>
#include <string_view>

namespace kvm::ir::task::detail {

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

}  // namespace kvm::ir::task::detail
