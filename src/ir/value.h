#pragma once

// Core value & operation PAYLOAD types of the KVM IR.
//
// This mirrors the ADT spec (docs / Notion "ir"):
//
//   Type      :: (name, dialect)
//   ValueImpl :: Any<"ValueImpl">          -- open, tagless mark
//   Value     :: (name, type, impl)
//   Operator  :: (name, dialect, inputs, outputs)
//   OperationImpl :: Any<"OperationImpl">  -- open, tagless mark
//   Operation :: (op, impl)
//
// These structs are PURE DATA: they carry no graph edges and no pointers. The
// topology (def / uses / operands / results / block membership) lives on the
// node layer (node.h) that WRAPS these payloads, and is managed by Block
// (graph.h). Keeping payload free of edges is what lets the ADT read like the
// ADT: a Value is just (name, type, impl), nothing more.
//
// The open "mark" (Any<...>) is an OPEN but BOUNDED sum: any dialect may add a
// member without the core knowing it (open), yet only types explicitly marked
// for that sum may be stored (bounded). This is realized by AnyOf<Mark> (see
// any_of.h): ValueImpl is the mark (the category), and AnyOf<ValueImpl> IS the
// sum -- a Value's `impl` field, matching the ADT's Any<"ValueImpl">.
//
// The mark carries no tag. `type` is NOT a key that indexes out an impl: there
// is no unified `GetImpl(type) -> Some` (a sum has no member outside a match,
// and a unified extractor would be inheritance, not a sum -- see any_of.h).
// `type` is a runtime CLASSIFICATION aligned with the impl; its role is to keep
// all values one uniform `Value` type (it does not extract anything). Recovery
// of an impl is a single-branch probe, `value.impl.as<ConcreteImpl>()`, written
// inside whatever match the caller is doing. Whether `type` and the stored
// member agree is a verify-pass concern, not enforced here.
//
// Register a member with KVM_MARK_MEMBER(ValueImpl, T) /
// KVM_MARK_MEMBER(OperationImpl, T) (see the builtin impls for examples).

#include <string>
#include <vector>

#include "any_of.h"

namespace kvm::ir {

// Type :: (name, dialect)
struct Type {
  std::string name;
  std::string dialect;
};

// ValueImpl -- the mark (the category "ValueImpl"). The open sum is
// AnyOf<ValueImpl>, matching the ADT's Any<"ValueImpl">.
struct ValueImpl {};

// Value :: (name, type, impl) -- pure payload (no edges; see node.h).
struct Value {
  std::string name;
  Type type;
  AnyOf<ValueImpl> impl;
};

// Operator :: (name, dialect, inputs, outputs)
// The abstract "kind/signature" of an operation: its name, dialect, and the
// types of its inputs/outputs. Reusable, carries no instance data.
struct Operator {
  std::string name;
  std::string dialect;
  std::vector<Type> inputs;
  std::vector<Type> outputs;
};

// OperationImpl -- the mark (the category "OperationImpl"). The open sum is
// AnyOf<OperationImpl>, matching the ADT's Any<"OperationImpl">.
struct OperationImpl {};

// Operation :: (op, impl) -- pure payload (no edges; see node.h).
// A concrete instance. `op` is the key (which kind); `impl` is the extra data
// for that kind (e.g. BlockOpImpl holds a Block).
struct Operation {
  Operator op;
  AnyOf<OperationImpl> impl;
};

}  // namespace kvm::ir
