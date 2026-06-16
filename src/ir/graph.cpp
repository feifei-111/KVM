#include "graph.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace kvm::ir {

void Graph::Bind(const Value* value, const Operation* op) { def_[value] = op; }

const Value* Graph::MakeInput(std::string name, Type type,
                              AnyOf<ValueImpl> impl) {
  // A level input has no defining operation: GetDef stays nullptr (Null). We
  // record nothing in def_; absence == Null.
  return arena_.AddValue(
      Value{std::move(name), std::move(type), std::move(impl)});
}

Block* Graph::MakeBlock(std::vector<const Value*> inputs) {
  return arena_.AddBlock(
      Block{/*operations=*/{}, std::move(inputs), /*outputs=*/{}});
}

const Operation* Graph::MakeOperation(Block* block, Operator op,
                                      AnyOf<OperationImpl> impl,
                                      std::vector<const Value*> inputs,
                                      std::vector<Value> outputs) {
  const Operation* operation =
      arena_.AddOperation(Operation{std::move(op), std::move(impl)});

  // Create the output values and bind each one's def to this operation.
  std::vector<const Value*> output_values;
  output_values.reserve(outputs.size());
  for (auto& out : outputs) {
    const Value* v = arena_.AddValue(std::move(out));
    Bind(v, operation);
    output_values.push_back(v);
  }

  // Record each input gaining this operation as a user.
  for (const Value* in : inputs) {
    users_[in].push_back(operation);
  }

  inputs_[operation] = std::move(inputs);
  outputs_[operation] = std::move(output_values);

  // Bind the operation to its block (creation-time membership) and append it in
  // program order. `block` is a mutable builder handle; readers only ever see
  // `const Block*`.
  op_block_[operation] = block;
  block->operations.push_back(operation);
  return operation;
}

void Graph::SetBlockOutputs(Block* block, std::vector<const Value*> outputs) {
  block->outputs = std::move(outputs);
}

const Block* Graph::GetBlock(const Operation* op) const {
  auto it = op_block_.find(op);
  return it == op_block_.end() ? nullptr : it->second;
}

void Graph::SetInput(const Operation* op, std::size_t index,
                     const Value* new_input) {
  auto it = inputs_.find(op);
  if (it == inputs_.end() || index >= it->second.size()) return;

  const Value* old_input = it->second[index];
  if (old_input == new_input) return;

  // Forward edge: repoint the operand.
  it->second[index] = new_input;

  // Reverse edge: drop one occurrence of `op` from old_input's users, add one
  // to new_input's. (An operand may appear multiple times; we move exactly the
  // one corresponding to this index.)
  auto& old_users = users_[old_input];
  for (auto u = old_users.begin(); u != old_users.end(); ++u) {
    if (*u == op) {
      old_users.erase(u);
      break;
    }
  }
  users_[new_input].push_back(op);
}

std::size_t Graph::ReplaceAllUses(const Value* old_value,
                                  const Value* new_value) {
  if (old_value == new_value) return 0;

  std::size_t rewritten = 0;
  // Snapshot users: SetInput mutates users_[old_value] as we go.
  auto it = users_.find(old_value);
  if (it == users_.end()) return 0;
  std::vector<const Operation*> affected = it->second;

  for (const Operation* op : affected) {
    auto in_it = inputs_.find(op);
    if (in_it == inputs_.end()) continue;
    for (std::size_t i = 0; i < in_it->second.size(); ++i) {
      if (in_it->second[i] == old_value) {
        SetInput(op, i, new_value);
        ++rewritten;
      }
    }
  }
  return rewritten;
}

const Operation* Graph::GetDef(const Value* value) const {
  auto it = def_.find(value);
  return it == def_.end() ? nullptr : it->second;
}

std::span<const Operation* const> Graph::GetUser(const Value* value) const {
  auto it = users_.find(value);
  if (it == users_.end()) return {};
  return it->second;
}

std::span<const Value* const> Graph::GetInputs(const Operation* op) const {
  auto it = inputs_.find(op);
  if (it == inputs_.end()) return {};
  return it->second;
}

std::span<const Value* const> Graph::GetOutputs(const Operation* op) const {
  auto it = outputs_.find(op);
  if (it == outputs_.end()) return {};
  return it->second;
}

namespace {
std::size_t Count(const std::vector<const Operation*>& v, const Operation* x) {
  std::size_t n = 0;
  for (const Operation* e : v)
    if (e == x) ++n;
  return n;
}
std::size_t Count(const std::vector<const Value*>& v, const Value* x) {
  std::size_t n = 0;
  for (const Value* e : v)
    if (e == x) ++n;
  return n;
}
}  // namespace

bool Graph::CheckConsistency(std::string* error) const {
  auto fail = [&](std::string msg) {
    if (error) *error = std::move(msg);
    return false;
  };

  // 1. def_/outputs_ agree: def_[v] == op  iff  v in outputs_[op].
  //    Forward -> reverse: every output's def points back to its op.
  for (const auto& [op, outs] : outputs_) {
    for (const Value* v : outs) {
      auto d = def_.find(v);
      if (d == def_.end() || d->second != op)
        return fail("output value has no matching def back-edge to its op");
    }
  }
  //    Reverse -> forward: every def entry's value is among that op's outputs.
  for (const auto& [v, op] : def_) {
    auto o = outputs_.find(op);
    if (o == outputs_.end() || Count(o->second, v) == 0)
      return fail("def points to an op that does not list the value as output");
  }

  // 2. inputs_/users_ agree, with multiplicity: the number of times `op` uses
  //    `v` as an input equals the number of times `op` appears in users_[v].
  for (const auto& [op, ins] : inputs_) {
    for (const Value* v : ins) {
      std::size_t as_input = Count(ins, v);
      auto u = users_.find(v);
      std::size_t as_user = (u == users_.end()) ? 0 : Count(u->second, op);
      if (as_input != as_user)
        return fail("input/user multiplicity mismatch between op and value");
    }
  }
  //    Reverse -> forward: every users_ entry is backed by a real input edge.
  for (const auto& [v, ops] : users_) {
    for (const Operation* op : ops) {
      auto in = inputs_.find(op);
      if (in == inputs_.end() || Count(in->second, v) == 0)
        return fail("user op does not list the value among its inputs");
    }
  }

  return true;
}

}  // namespace kvm::ir
