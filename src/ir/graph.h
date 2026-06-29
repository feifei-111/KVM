#pragma once

// Graph structure for the KVM IR.
//
// The ADT declares the graph relations as FUNCTIONS, not fields:
//
//   GetDef     :: Value -> (Operation | Null)
//   GetUser    :: Value -> [Operation]
//   GetInputs  :: Operation -> [Value]
//   GetOutputs :: Operation -> [Value]
//
// So Value/Operation (value.h) carry no edges. Graph owns the IR: an arena
// (Context) holds the real nodes, and Graph keeps the edges in side tables; the
// Get* methods read those tables. Nodes refer to each other by non-owning
// `const T*` -- consistent with the VM assumptions (immutable, immortal, freely
// shared): the arena outlives all nodes, so raw pointers never dangle and there
// are no ownership cycles.
//
// Edges are stored REDUNDANTLY (forward: op->inputs/outputs; reverse:
// value->def/users), like MLIR's operand lists + use lists. Redundancy is safe
// because there is a SINGLE atomic write path: building goes through
// MakeOperation, and mutation (graph transforms) goes through SetInput /
// ReplaceAllUses, each of which updates both directions together. Everything
// returned to callers is const, so the two directions cannot drift from
// outside. As a belt-and-suspenders measure, CheckConsistency() verifies
// forward and reverse agree.
//
// GetDef returns `const Operation*`; nullptr is the ADT's Null, meaning "this
// value is an input at its level" (it has no defining operation).
//
// Blocks: every operation belongs to exactly one block, bound at creation
// (MakeOperation takes the block). Edges remain global (option A) -- a Block
// only groups membership and declares its interface. Nesting follows scheme
// "jia": an inner block is built first, then referenced by a block-op
// (BlockOpImpl) created in the outer block. Graph.main is the one root block,
// owned by no block-op.

#include <cstddef>
#include <deque>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "graph_config.h"
#include "value.h"

namespace kvm::ir {

// BlockOpImpl<OperationImpl> :: (block: Block)
// The operation impl that carries a nested block. An operation whose op is a
// block-op holds its child block here (scheme "jia"). This is the structural
// member that makes nesting possible; if/for ops will reuse the same pattern.
struct BlockOpImpl {
  const Block* block = nullptr;
};

}  // namespace kvm::ir

KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::BlockOpImpl);

namespace kvm::ir {

// Context: the arena tool a Graph uses to own node storage. It only holds the
// real Value/Operation/Block objects (stable addresses via deque) and hands out
// pointers. Edges and all graph semantics live on Graph, not here.
class Context {
 public:
  Context() = default;
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  // Add a node to the arena. If its `name` is empty, a default label is filled
  // in (v0, v1, ... / block0, block1, ...). A creator that supplies a name
  // (parse, a naming pass) keeps it; the auto-name only kicks in for hand-built
  // graphs that omit names. Operations have no name.
  Value* AddValue(Value value) {
    if (value.name.empty()) value.name = "v" + std::to_string(value_id_++);
    values_.push_back(std::move(value));
    return &values_.back();
  }
  Operation* AddOperation(Operation op) {
    operations_.push_back(std::move(op));
    return &operations_.back();
  }
  Block* AddBlock(Block block) {
    if (block.name.empty()) block.name = "block" + std::to_string(block_id_++);
    blocks_.push_back(std::move(block));
    return &blocks_.back();
  }

 private:
  // deque keeps element addresses stable as the arena grows, so the pointers
  // handed out (and the edges referring to them) remain valid.
  std::deque<Value> values_;
  std::deque<Operation> operations_;
  std::deque<Block> blocks_;

  // Monotonic counters for default names (per kind, so v0.. and block0.. are
  // each contiguous). No uniqueness check against creator-supplied names.
  std::size_t value_id_ = 0;
  std::size_t block_id_ = 0;
};

// Graph: the IR object passes work with. It owns an arena (Context) and the
// global edge tables, and provides building / query / transform on top.
//
// ADT: Graph :: (main: Block, config). `main` is the one root block, owned by
// no block-op.
//
// Mutability: building uses mutable Block* handles internally; everything
// returned to callers (GetDef/GetUser/.../main) is const, so the only way to
// change edges is the atomic write path here (MakeOperation / SetInput /
// ReplaceAllUses).
class Graph {
 public:
  Graph() = default;
  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  // --- building ---

  // Create a free-standing value with no defining operation (GetDef ==
  // nullptr). Use for level inputs (e.g. graph / block inputs).
  const Value* MakeInput(std::string name, Type type,
                         AnyOf<ValueImpl> impl = {});

  // Create an empty block. `name` may be empty (auto-filled block0, block1...).
  // Returns a mutable builder handle; operations are added by
  // MakeOperation(block, ...) and outputs declared via SetBlockOutputs.
  // (Scheme "jia": build an inner block fully, then wrap it in a block-op.)
  Block* MakeBlock(std::vector<const Value*> inputs = {},
                   std::string name = "");

  // Create an operation INSIDE `block` -- binding to a block is part of
  // creation, so no operation is ever free-floating. The op is appended to
  // block->operations in program order. `outputs` gives the output values as
  // plain data (name/type/impl -- a Value carries no edges); the arena copies
  // each in and binds its def to this operation, then records all edges (each
  // input gains this op as a user). Returns the operation; its created output
  // values are retrieved via GetOutputs.
  const Operation* MakeOperation(Block* block, Operator op,
                                 AnyOf<OperationImpl> impl,
                                 std::vector<const Value*> inputs,
                                 std::vector<Value> outputs);

  // Declare a block's exposed outputs (values produced by ops inside it).
  void SetBlockOutputs(Block* block, std::vector<const Value*> outputs);

  // Set the root block (ADT: Graph.main).
  void SetMain(const Block* main) { main_ = main; }

  GraphConfig& config() { return config_; }

  // --- graph information (the ADT functions); all return const ---

  const Block* main() const { return main_; }
  const GraphConfig& config() const { return config_; }

  // GetBlock :: Operation -> (Block | Null). The block an operation belongs to.
  const Block* GetBlock(const Operation* op) const;

  // GetDef :: Value -> (Operation | Null). nullptr == Null (a level input).
  const Operation* GetDef(const Value* value) const;

  // GetUser :: Value -> [Operation].
  std::span<const Operation* const> GetUser(const Value* value) const;

  // GetInputs :: Operation -> [Value].
  std::span<const Value* const> GetInputs(const Operation* op) const;

  // GetOutputs :: Operation -> [Value].
  std::span<const Value* const> GetOutputs(const Operation* op) const;

  // --- graph transforms (the single atomic write path for mutation) ---

  // Repoint one input of `op` to `new_input`, updating both the forward edge
  // (inputs_) and the reverse edge (users_) together. This is the only way to
  // change a use, mirroring MLIR's OpOperand::set().
  void SetInput(const Operation* op, std::size_t index, const Value* new_input);

  // Replace every use of `old_value` with `new_value` across the whole graph
  // (RAUW). Returns the number of uses rewritten.
  std::size_t ReplaceAllUses(const Value* old_value, const Value* new_value);

  // --- invariants ---

  // Verify the redundant forward/reverse edges agree:
  //   def_[v] == op            iff  v in outputs_[op]
  //   op in users_[v]          iff  v in inputs_[op]
  // Returns true if consistent; otherwise writes a description to `error` (if
  // non-null) and returns false.
  bool CheckConsistency(std::string* error = nullptr) const;

 private:
  // Bind a value's def to an operation (records the def_ reverse edge). Used
  // internally when an operation creates its outputs.
  void Bind(const Value* value, const Operation* op);

  Context arena_;

  // Global edge tables (option A: Value/Operation stay pure data).
  std::unordered_map<const Value*, const Operation*> def_;
  std::unordered_map<const Value*, std::vector<const Operation*>> users_;
  std::unordered_map<const Operation*, std::vector<const Value*>> inputs_;
  std::unordered_map<const Operation*, std::vector<const Value*>> outputs_;

  // Which block each operation belongs to (bound at creation).
  std::unordered_map<const Operation*, const Block*> op_block_;

  const Block* main_ = nullptr;
  GraphConfig config_;
};

}  // namespace kvm::ir
