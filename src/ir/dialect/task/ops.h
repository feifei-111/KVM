#pragma once

// Operators of the task-graph dialect: the OperationImpls it defines.
//
//   ReduceOp    :: (axis: Int)   -- task.warp.reduce.sum / .max
//   BroadcastOp :: (axis: Int)   -- task.warp.broadcast
//
// Most task ops need no impl (their signature + operand locs are enough). Data
// movement is named by direction (task.tma.g2s, task.lsu.s2r, ...) because each
// direction is a different hardware instruction, so no CopyOp is needed. Only
// reduce/broadcast carry a constant the op name can't express: an axis. reduce
// stays a single semantic op (not lowered into a shfl + add/max tree here)
// because its hardware form needs lane/warp topology the task level hasn't
// bound yet -- same rule as mma. Text form (inside `<...>`): "axis=1".

#include <string>
#include <string_view>

#include "value.h"

namespace kvm::ir::task {

namespace detail {
std::string FormatAxis(int axis);
int ParseAxis(std::string_view text);
}  // namespace detail

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

}  // namespace kvm::ir::task

KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::task::ReduceOp);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::task::BroadcastOp);
