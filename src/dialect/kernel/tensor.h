#pragma once

// TensorImpl<ValueImpl> :: (shape, stride, dtype)
//
// The tensor value's impl in the kernel-graph dialect. Registered into the
// ValueImpl mark and given a text codec (Serialize/Deserialize) so it round
// trips through the serializer. Text form (inside `<...>`):
//   dtype=f16, shape=[8,4096], stride=[4096,1]
// stride is omitted on output when empty, and optional on input.

#include <string>
#include <string_view>
#include <vector>

#include "dialect/kernel/dtype.h"
#include "dialect/kernel/text_util.h"
#include "value.h"

namespace kvm::ir::kernel {

struct TensorImpl {
  std::vector<int> shape;
  std::vector<int> stride;
  Dtype dtype = Dtype::kF32;

  std::string Serialize() const {
    std::string out = "dtype=" + DtypeName(dtype);
    out += ", shape=" + text::FormatInts(shape);
    if (!stride.empty()) out += ", stride=" + text::FormatInts(stride);
    return out;
  }

  static TensorImpl Deserialize(std::string_view text) {
    TensorImpl t;
    // fields are comma-separated key=value; values may contain bracketed lists.
    std::size_t i = 0;
    while (i < text.size()) {
      // read key
      std::size_t key_start = i;
      while (i < text.size() && text[i] != '=') ++i;
      std::string_view key = text.substr(key_start, i - key_start);
      if (i < text.size()) ++i;  // skip '='
      // read value up to a top-level comma (respect brackets)
      std::size_t val_start = i;
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
      std::string_view val = text.substr(val_start, i - val_start);
      if (i < text.size()) ++i;  // skip ','

      auto trim = [](std::string_view s) {
        std::size_t a = 0, b = s.size();
        while (a < b && s[a] == ' ') ++a;
        while (b > a && s[b - 1] == ' ') --b;
        return s.substr(a, b - a);
      };
      key = trim(key);
      val = trim(val);
      if (key == "dtype")
        t.dtype = DtypeFromName(val);
      else if (key == "shape")
        t.shape = text::ParseInts(val);
      else if (key == "stride")
        t.stride = text::ParseInts(val);
    }
    return t;
  }
};

}  // namespace kvm::ir::kernel

KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::kernel::TensorImpl);
