#pragma once

// Values of the task-graph dialect: the ValueImpls it defines.
//
//   Location   :: Enum("global", "shared", "register")
//   TensorImpl :: (shape, stride, dtype, loc)
//
// Same as the kernel dialect's tensor, plus a storage `loc` -- the one piece of
// semantics the task level adds (a kernel value is just an algebraic tensor; a
// task value must say WHERE it lives, else global/shared/register are
// indistinguishable). loc is hardware information conceptually owned by the
// device; it sits on the value only because the ADT is written that way. Dtype
// and the int-list text helpers come from the shared dialect utilities. Text
// form (inside `<...>`):
//   dtype=f16, loc=shared, shape=[8,4096], stride=[4096,1]
// stride is omitted on output when empty; loc is always printed.

#include <string>
#include <string_view>
#include <vector>

#include "dialect/dtype.h"
#include "dialect/text_util.h"
#include "value.h"

namespace kvm::ir::task {

using dialect::Dtype;
namespace text = dialect::text;

enum class Location { kGlobal, kShared, kRegister };

inline std::string LocationName(Location l) {
  switch (l) {
    case Location::kGlobal:
      return "global";
    case Location::kShared:
      return "shared";
    case Location::kRegister:
      return "register";
  }
  return "global";
}

inline Location LocationFromName(std::string_view s) {
  if (s == "shared") return Location::kShared;
  if (s == "register") return Location::kRegister;
  return Location::kGlobal;
}

struct TensorImpl {
  std::vector<int> shape;
  std::vector<int> stride;
  Dtype dtype = Dtype::kF32;
  Location loc = Location::kGlobal;

  std::string Serialize() const {
    std::string out = "dtype=" + dialect::DtypeName(dtype);
    out += ", loc=" + LocationName(loc);
    out += ", shape=" + text::FormatInts(shape);
    if (!stride.empty()) out += ", stride=" + text::FormatInts(stride);
    return out;
  }

  static TensorImpl Deserialize(std::string_view text_in) {
    TensorImpl t;
    // fields are comma-separated key=value; values may contain bracketed lists.
    std::size_t i = 0;
    while (i < text_in.size()) {
      std::size_t key_start = i;
      while (i < text_in.size() && text_in[i] != '=') ++i;
      std::string_view key = text_in.substr(key_start, i - key_start);
      if (i < text_in.size()) ++i;  // skip '='
      std::size_t val_start = i;
      int depth = 0;
      for (; i < text_in.size(); ++i) {
        char c = text_in[i];
        if (c == '[')
          ++depth;
        else if (c == ']')
          --depth;
        else if (c == ',' && depth == 0)
          break;
      }
      std::string_view val = text_in.substr(val_start, i - val_start);
      if (i < text_in.size()) ++i;  // skip ','

      auto trim = [](std::string_view s) {
        std::size_t a = 0, b = s.size();
        while (a < b && s[a] == ' ') ++a;
        while (b > a && s[b - 1] == ' ') --b;
        return s.substr(a, b - a);
      };
      key = trim(key);
      val = trim(val);
      if (key == "dtype")
        t.dtype = dialect::DtypeFromName(val);
      else if (key == "loc")
        t.loc = LocationFromName(val);
      else if (key == "shape")
        t.shape = text::ParseInts(val);
      else if (key == "stride")
        t.stride = text::ParseInts(val);
    }
    return t;
  }
};

}  // namespace kvm::ir::task

KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::task::TensorImpl);
