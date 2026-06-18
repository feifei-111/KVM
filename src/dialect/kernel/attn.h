#pragma once

// Attention meta value for the kernel-graph dialect.
//
//   AttnMask      :: Enum("None", "Causal")
//   AttnBatchDesc :: (offsets: [Int], positions: [Int])
//   KVBatchDesc   :: (desc: AttnBatchDesc, block_table: [[Int]])
//   AttnMetaImpl  :: (q_batch, kv_batch, mask)
//
// This is a value (an input), because the batch composition / block table are
// dynamic per step. Text form (inside `<...>`):
//   q={off=[..],pos=[..]}, kv={off=[..],pos=[..],blocks=[[..],..]}, mask=causal

#include <string>
#include <string_view>
#include <vector>

#include "dialect/kernel/text_util.h"
#include "value.h"

namespace kvm::ir::kernel {

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

KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::kernel::AttnMetaImpl);
