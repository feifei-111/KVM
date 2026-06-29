#pragma once

// Values of the kernel-graph dialect: the ValueImpls it defines.
//
//   TensorImpl   :: (shape, stride, dtype)
//   AttnMetaImpl :: (q_batch, kv_batch, mask)
//
// A dialect is a set of value + op definitions; this header is the "values"
// half for `kernel`. Dtype and the int-list text helpers come from the shared
// dialect utilities. Tensor text form (inside `<...>`):
//   dtype=f16, shape=[8,4096], stride=[4096,1]
// stride is omitted on output when empty. AttnMeta is a value (an input)
// because its batch composition / block table are dynamic per step; its longer
// text codec lives in values.cpp.

#include <string>
#include <string_view>
#include <vector>

#include "dialect/dtype.h"
#include "dialect/text_util.h"
#include "value.h"

namespace kvm::ir::kernel {

using dialect::Dtype;
namespace text = dialect::text;

struct TensorImpl {
  std::vector<int> shape;
  std::vector<int> stride;
  Dtype dtype = Dtype::kF32;

  std::string Serialize() const {
    std::string out = "dtype=" + dialect::DtypeName(dtype);
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
      else if (key == "shape")
        t.shape = text::ParseInts(val);
      else if (key == "stride")
        t.stride = text::ParseInts(val);
    }
    return t;
  }
};

// --- attention meta value ---
//
//   AttnMask      :: Enum("None", "Causal")
//   AttnBatchDesc :: (offsets: [Int], positions: [Int])
//   KVBatchDesc   :: (desc: AttnBatchDesc, block_table: [[Int]])
// Text form: q={off=[..],pos=[..]}, kv={off=[..],pos=[..],blocks=[[..],..]},
//            mask=causal

enum class AttnMask { kNone, kCausal };

inline std::string MaskName(AttnMask m) {
  return m == AttnMask::kCausal ? "causal" : "none";
}
inline AttnMask MaskFromName(std::string_view s) {
  return s == "causal" ? AttnMask::kCausal : AttnMask::kNone;
}

struct AttnBatchDesc {
  std::vector<int> offsets;
  std::vector<int> positions;
};

struct KVBatchDesc {
  AttnBatchDesc desc;
  std::vector<std::vector<int>> block_table;
};

struct AttnMetaImpl {
  AttnBatchDesc q_batch;
  KVBatchDesc kv_batch;
  AttnMask mask = AttnMask::kNone;

  std::string Serialize() const;
  static AttnMetaImpl Deserialize(std::string_view text);
};

}  // namespace kvm::ir::kernel

KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::kernel::TensorImpl);
KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::kernel::AttnMetaImpl);
