#pragma once

// Topology layer for the KVM IR.
//
// value.h holds pure PAYLOAD (Value / Operation): no edges, no pointers. This
// header wraps each payload in a NODE that carries the topology -- the edges
// the ADT describes as functions (def / uses / operands / results). A node
// holds its payload by value (settable via the mutable value()/op() accessor)
// and the edges as pointers between nodes. The Arena (graph.h) OWNS node
// storage; Block (graph.h) manages the topology and is the only writer of these
// edges (the single atomic write path), so the redundant forward/reverse links
// (def<->results, operands<->uses) cannot drift.
//
//   ValueNode :: payload Value,     def: OpNode|Null (Null == block argument),
//                uses: [Use]
//   OpNode    :: payload Operation, operands: [ValueNode], results:
//   [ValueNode],
//                block: Block
//
// "Pointers live on nodes" is exactly what makes the IR convenient to mutate:
// to walk from a value to its users, or rewrite an operand, you follow a node
// link, not a lookup in a side table kept far from the node.

#include <cstddef>
#include <vector>

#include "value.h"

namespace kvm::ir {

class OpNode;
class Block;

// A single use: operand slot `index` of operation `user` reads the value.
struct Use {
  OpNode* user = nullptr;
  std::size_t index = 0;
};

// ValueNode: a Value payload plus its def/use edges. Created either as a
// block argument (def == nullptr) or as an operation result (def == that op).
class ValueNode {
 public:
  const Value& value() const { return value_; }
  Value& value() { return value_; }

  // def: the operation producing this value; nullptr == it is a block
  // argument (a level input, the ADT's Null).
  const OpNode* def() const { return def_; }
  OpNode* def() { return def_; }

  // The operations (and operand slots) that read this value.
  const std::vector<Use>& uses() const { return uses_; }

 private:
  friend class Block;
  friend class Arena;
  Value value_;
  OpNode* def_ = nullptr;
  std::vector<Use> uses_;
};

// OpNode: an Operation payload plus its operand/result edges and the block it
// belongs to (bound at creation -- no op is ever free-floating).
class OpNode {
 public:
  const Operation& op() const { return op_; }
  Operation& op() { return op_; }

  const std::vector<ValueNode*>& operands() const { return operands_; }
  const std::vector<ValueNode*>& results() const { return results_; }
  const Block* block() const { return block_; }
  Block* block() { return block_; }

 private:
  friend class Block;
  friend class Arena;
  Operation op_;
  std::vector<ValueNode*> operands_;
  std::vector<ValueNode*> results_;
  Block* block_ = nullptr;
};

}  // namespace kvm::ir
