#pragma once

// Operators of the kernel-graph dialect: the OperationImpls it defines.
//
// Currently the distribution / collective ops, which carry rank constants the
// op name alone can't express (a `send` is meaningless without from/to). These
// are static graph constants, so they live on the OperationImpl (not as value
// inputs). Most kernel ops need no impl (their signature is enough); only these
// do, so this is the "ops" half of the dialect.
//
//   SendOp/RecvOp   :: (from: Int, to: Int)
//   BroadcastOp     :: (from: Int, to: [Int])
//   AllToAllOp / AllReduceSum / AllReduceMax / AllGather /
//   MoeDispatch / MoeCombine :: (ranks: [Int])
//
// Text form mirrors the field shapes, e.g. "from=0, to=1" or "ranks=[0,1,2,3]".

#include <string>
#include <string_view>
#include <vector>

#include "dialect/text_util.h"
#include "value.h"

namespace kvm::ir::kernel {

namespace text = dialect::text;

namespace detail {
// shared helpers for "from=I, to=I" and "ranks=[...]" forms
std::string FormatFromTo(int from, int to);
void ParseFromTo(std::string_view text, int& from, int& to);
std::string FormatRanks(const std::vector<int>& ranks);
std::vector<int> ParseRanks(std::string_view text);
std::string FormatBroadcast(int from, const std::vector<int>& to);
void ParseBroadcast(std::string_view text, int& from, std::vector<int>& to);
}  // namespace detail

struct SendOp {
  int from = 0;
  int to = 0;
  std::string Serialize() const { return detail::FormatFromTo(from, to); }
  static SendOp Deserialize(std::string_view t) {
    SendOp o;
    detail::ParseFromTo(t, o.from, o.to);
    return o;
  }
};

struct RecvOp {
  int from = 0;
  int to = 0;
  std::string Serialize() const { return detail::FormatFromTo(from, to); }
  static RecvOp Deserialize(std::string_view t) {
    RecvOp o;
    detail::ParseFromTo(t, o.from, o.to);
    return o;
  }
};

struct BroadcastOp {
  int from = 0;
  std::vector<int> to;
  std::string Serialize() const { return detail::FormatBroadcast(from, to); }
  static BroadcastOp Deserialize(std::string_view t) {
    BroadcastOp o;
    detail::ParseBroadcast(t, o.from, o.to);
    return o;
  }
};

// Collectives that just carry a rank set. Macro keeps the six identical bodies
// from drifting; each is still its own distinct type (its own mark member).
#define KVM_KERNEL_RANKS_OP(Name)                                        \
  struct Name {                                                          \
    std::vector<int> ranks;                                              \
    std::string Serialize() const { return detail::FormatRanks(ranks); } \
    static Name Deserialize(std::string_view t) {                        \
      return Name{detail::ParseRanks(t)};                                \
    }                                                                    \
  }

KVM_KERNEL_RANKS_OP(AllToAllOp);
KVM_KERNEL_RANKS_OP(AllReduceSum);
KVM_KERNEL_RANKS_OP(AllReduceMax);
KVM_KERNEL_RANKS_OP(AllGather);
KVM_KERNEL_RANKS_OP(MoeDispatch);
KVM_KERNEL_RANKS_OP(MoeCombine);

#undef KVM_KERNEL_RANKS_OP

}  // namespace kvm::ir::kernel

KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::SendOp);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::RecvOp);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::BroadcastOp);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::AllToAllOp);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::AllReduceSum);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::AllReduceMax);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::AllGather);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::MoeDispatch);
KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::kernel::MoeCombine);
