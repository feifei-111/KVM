#pragma once

// Core value & operation types of the KVM IR.
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
//
// Graph edges (def / users / inputs / outputs) are NOT fields here; they are
// functions, defined in graph.h. These structs only hold intrinsic data.

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

// Value :: (name, type, impl)
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

// Operation :: (op, impl)
// A concrete instance. `op` is the key (which kind); `impl` is the extra data
// for that kind (e.g. BlockOpImpl holds a Block).
struct Operation {
  Operator op;
  AnyOf<OperationImpl> impl;
};

// Block :: (operations, inputs, outputs)
// A block is a sequence of operations plus its interface. Edges live globally
// in the Context (option A); a Block just groups membership and declares its
// interface:
//   - operations: the ops belonging to this block, in program order. Every
//     operation belongs to exactly one block (bound at creation).
//   - inputs:  level inputs of this block; their GetDef == nullptr (Null).
//   - outputs: values produced inside the block that it exposes.
//   - name:    a label to tell block instances apart (like Value.name); given
//     by the creator, or auto-filled (block0, block1, ...) when left empty.
// Nodes are referred to by non-owning pointers into the Context arena.
struct Block {
  std::string name;
  std::vector<const Operation*> operations;
  std::vector<const Value*> inputs;
  std::vector<const Value*> outputs;
};

}  // namespace kvm::ir
