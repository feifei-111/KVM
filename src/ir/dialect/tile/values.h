#pragma once

// Values of the tile-graph dialect: the ValueImpls it defines.
//
//   Location   :: Enum("global", "shared", "register")
//   Slice      :: [Int]
//   TensorImpl :: (shape, stride, dtype, loc, slice)
//
// Same as the task dialect's tensor, plus a `slice` -- a purely logical
// descriptor (which region of the source task tensor this tile came from), used
// for dependency analysis (name + slice decides who depends on whom). slice
// does NOT take part in an op's hardware legality check; op constraints look
// only at shape/dtype/loc. slice is "where this block was cut from", shape is
// "how big this block is" -- the two are orthogonal. Dtype and the int-list
// text helpers come from the shared dialect utilities. Text form (inside
// `<...>`):
//   dtype=f16, loc=shared, shape=[8,16], stride=[16,1], slice=[0,8]
// stride is omitted on output when empty; loc is always printed; slice is
// omitted when empty.

#include <string>
#include <string_view>
#include <vector>

#include "dialect/dtype.h"
#include "dialect/text_util.h"
#include "value.h"

namespace kvm::ir::tile {

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
  std::vector<int> slice;

  std::string Serialize() const {
    std::string out = "dtype=" + dialect::DtypeName(dtype);
    out += ", loc=" + LocationName(loc);
    out += ", shape=" + text::FormatInts(shape);
    if (!stride.empty()) out += ", stride=" + text::FormatInts(stride);
    if (!slice.empty()) out += ", slice=" + text::FormatInts(slice);
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
      else if (key == "slice")
        t.slice = text::ParseInts(val);
    }
    return t;
  }
};

}  // namespace kvm::ir::tile

KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::tile::TensorImpl);
