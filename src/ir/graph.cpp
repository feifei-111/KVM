#include "graph.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace kvm::ir {

ValueNode* Block::AddArgument(Value value) {
  ValueNode* v = arena_->NewValue(std::move(value));
  // A block argument has no defining op: def_ stays nullptr (the ADT's Null).
  arguments_.push_back(v);
  return v;
}

OpNode* Block::MakeOperation(Operation op, std::vector<ValueNode*> operands,
                             std::vector<Value> results) {
  OpNode* node = arena_->NewOp(std::move(op));
  node->block_ = this;

  // Forward operand edges + reverse use edges.
  node->operands_ = std::move(operands);
  for (std::size_t i = 0; i < node->operands_.size(); ++i) {
    AddUse(node->operands_[i], node, i);
  }

  // Allocate result values; each one's def is this op.
  node->results_.reserve(results.size());
  for (auto& payload : results) {
    ValueNode* r = arena_->NewValue(std::move(payload));
    r->def_ = node;
    node->results_.push_back(r);
  }

  operations_.push_back(node);
  return node;
}

void Block::AddUse(ValueNode* value, OpNode* op, std::size_t index) {
  value->uses_.push_back(Use{op, index});
}

void Block::RemoveUse(ValueNode* value, OpNode* op, std::size_t index) {
  auto& uses = value->uses_;
  for (auto it = uses.begin(); it != uses.end(); ++it) {
    if (it->user == op && it->index == index) {
      uses.erase(it);
      return;
    }
  }
}

void Block::SetOperand(OpNode* op, std::size_t index, ValueNode* new_value) {
  if (index >= op->operands_.size()) return;
  ValueNode* old_value = op->operands_[index];
  if (old_value == new_value) return;

  RemoveUse(old_value, op, index);
  op->operands_[index] = new_value;
  AddUse(new_value, op, index);
}

std::size_t Block::ReplaceAllUses(ValueNode* old_value, ValueNode* new_value) {
  if (old_value == new_value) return 0;
  // Snapshot: SetOperand mutates old_value->uses_ as we go.
  std::vector<Use> affected = old_value->uses_;
  for (const Use& u : affected) SetOperand(u.user, u.index, new_value);
  return affected.size();
}

void Block::EraseOp(OpNode* op) {
  // Detach from operands' use lists.
  for (std::size_t i = 0; i < op->operands_.size(); ++i) {
    RemoveUse(op->operands_[i], op, i);
  }
  op->operands_.clear();
  // Drop from program order.
  for (auto it = operations_.begin(); it != operations_.end(); ++it) {
    if (*it == op) {
      operations_.erase(it);
      break;
    }
  }
}

}  // namespace kvm::ir
