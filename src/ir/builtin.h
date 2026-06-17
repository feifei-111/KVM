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
//
// Each impl owns its textual form via two static-ish methods:
//   std::string Serialize() const;          -- the text inside `<...>`
//   static T     Deserialize(string_view);  -- build T from that text
// The impl only knows about ITSELF (not AnyOf / the open sum). Wrapping the
// concrete impl back into AnyOf<ValueImpl> is the serialization framework's
// job, done where the concrete type T is known.

#include <cstdint>
#include <string>
#include <string_view>

#include "value.h"

namespace kvm::ir {

// ConstBoolImpl<ValueImpl> :: (value: Bool)
struct ConstBoolImpl {
  bool value;
  std::string Serialize() const { return value ? "true" : "false"; }
  static ConstBoolImpl Deserialize(std::string_view text) {
    return ConstBoolImpl{text == "true"};
  }
};

// ConstIntImpl<ValueImpl> :: (value: Int)
struct ConstIntImpl {
  std::int64_t value;
  std::string Serialize() const { return std::to_string(value); }
  static ConstIntImpl Deserialize(std::string_view text) {
    return ConstIntImpl{std::stoll(std::string(text))};
  }
};

// ConstFloatImpl<ValueImpl> :: (value: Float)
struct ConstFloatImpl {
  double value;
  std::string Serialize() const { return std::to_string(value); }
  static ConstFloatImpl Deserialize(std::string_view text) {
    return ConstFloatImpl{std::stod(std::string(text))};
  }
};

}  // namespace kvm::ir

KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::ConstBoolImpl);
KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::ConstIntImpl);
KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::kvm::ir::ConstFloatImpl);
