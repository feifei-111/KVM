#pragma once

// Operators of the tile-graph dialect: the OperationImpls it defines.
//
//   MmaOp / MmaAccOp :: (m, n, k)   -- tile.tc.mma / tile.tc.mma_acc
//   ReduceOp         :: (axis: Int) -- tile.warp.reduce.sum / .max
//   BroadcastOp      :: (axis: Int) -- tile.warp.broadcast
//
// Unlike task, a tile op is aligned to hardware: it carries the concrete tile
// shape it realizes. mma/mma_acc keep that shape (m,n,k) on their impl -- a
// tile variant is a parameter, not a separate op name. Data movement is still
// named by direction (tile.tma.gmem_to_smem, ...), since each direction is a
// distinct instruction. reduce/broadcast carry an axis. Text form: "m=64,
// n=128, k=16" or "axis=1".

#include <string>
#include <string_view>

#include "value.h"

namespace kvm::ir::tile {

namespace detail {
std::string FormatAxis(int axis);
int ParseAxis(std::string_view text);
int IntField(std::string_view text, std::string_view key);
}  // namespace detail

// TC matmul tile ops carry the hardware tile shape (m,n,k) they realize -- the
// shape is a parameter of the op instance (a tile variant), not encoded in the
// op name. mma and mma_acc are kept separate: mma_acc takes an extra C input,
// a real semantic difference, so they are distinct ops with distinct impls.
// Text form (inside `<...>`): "m=64, n=128, k=16".
struct MmaOp {
  int m = 0, n = 0, k = 0;
  std::string Serialize() const {
    return "m=" + std::to_string(m) + ", n=" + std::to_string(n) +
           ", k=" + std::to_string(k);
  }
  static MmaOp Deserialize(std::string_view t) {
    return MmaOp{detail::IntField(t, "m"), detail::IntField(t, "n"),
                 detail::IntField(t, "k")};
  }
};

struct MmaAccOp {
  int m = 0, n = 0, k = 0;
  std::string Serialize() const {
    return "m=" + std::to_string(m) + ", n=" + std::to_string(n) +
           ", k=" + std::to_string(k);
  }
  static MmaAccOp Deserialize(std::string_view t) {
    return MmaAccOp{detail::IntField(t, "m"), detail::IntField(t, "n"),
                    detail::IntField(t, "k")};
  }
};

struct ReduceOp {
  int axis = 0;
  std::string Serialize() const { return detail::FormatAxis(axis); }
  static ReduceOp Deserialize(std::string_view t) {
    return ReduceOp{detail::ParseAxis(t)};
  }
};

struct BroadcastOp {
  int axis = 0;
  std::string Serialize() const { return detail::FormatAxis(axis); }
  static BroadcastOp Deserialize(std::string_view t) {
    return BroadcastOp{detail::ParseAxis(t)};
  }
};

}  // namespace kvm::ir::tile

KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::tile::MmaOp);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::tile::MmaAccOp);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::tile::ReduceOp);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::tile::BroadcastOp);
