#pragma once

// Convenience: register the builtin value impls' codecs into a Registry.
// Dialects do the same for their own impls (one RegisterValueImpl / line).

#include "builtin.h"
#include "serialization/registry.h"

namespace kvm::ir::serial {

inline void RegisterBuiltins(Registry& r) {
  r.RegisterValueImpl<ConstBoolImpl>("builtin.bool");
  r.RegisterValueImpl<ConstIntImpl>("builtin.int");
  r.RegisterValueImpl<ConstFloatImpl>("builtin.float");
}

}  // namespace kvm::ir::serial
