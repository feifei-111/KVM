#pragma once

// Builtin value impls for the KVM IR.
//
// ADT (BuiltinValue):
//   ConstBoolImpl<ValueImpl>  :: (value: Bool)
//   ConstIntImpl<ValueImpl>   :: (value: Int)
//   ConstFloatImpl<ValueImpl> :: (value: Float)
//
// Each is a plain data struct registered into the ValueImpl mark with one line
// (KVM_MARK_MEMBER). That registration is the whole story of "open": a value's
// impl can be any registered member, and members are added without touching the
// core. Recovery is `value.impl.as<ConstIntImpl>()`, with `type` as the key
// that says which member to ask for.

#include <cstdint>

#include "value.h"

namespace kvm::ir {

// ConstBoolImpl<ValueImpl> :: (value: Bool)
struct ConstBoolImpl {
  bool value;
};

// ConstIntImpl<ValueImpl> :: (value: Int)
struct ConstIntImpl {
  std::int64_t value;
};

// ConstFloatImpl<ValueImpl> :: (value: Float)
struct ConstFloatImpl {
  double value;
};

}  // namespace kvm::ir

KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::ConstBoolImpl);
KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::ConstIntImpl);
KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::ConstFloatImpl);
