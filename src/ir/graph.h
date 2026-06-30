#pragma once

// Graph structure for the KVM IR: ownership (Arena), topology (Block), and the
// top-level aggregate (Graph).
//
// Layering (settled design):
//   - Arena OWNS all node storage -- every ValueNode, OpNode, and Block lives
//     in one arena, hung off the Graph. Stable addresses (deque) let nodes
//     refer to each other by raw pointer with no dangling and no cycles.
//   - Block MANAGES TOPOLOGY for its own level: the op order, its argument and
//     output value lists, and the def/use/operand/result edges among the nodes
//     it contains. A Block does not own nodes; it asks the arena for storage
//     and then wires the edges. This is the single atomic write path, so the
//     redundant forward/reverse edges on nodes cannot drift.
//   - Graph is just (root: Block, config). It holds the arena and the root
//     block; it has no building methods of its own -- you build through a
//     Block.
//
// Blocks are unrelated to each other: nesting is expressed ONLY by a BlockOp
// inside one block referencing another block (a one-way reference, like an
// operand edge). A block never records who references it. The root block is
// simply the one no BlockOp references.
//
// A value's def is an OpNode, or nullptr meaning "this value is a block
// argument" (a level input -- the ADT's Null).

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#include "graph_config.h"
#include "node.h"
#include "value.h"

namespace kvm::ir {

class Block;

// BlockOpImpl<OperationImpl> :: (block: Block)
// The operation impl that carries a referenced block. An operation whose op is
// a block-op holds the block it references here. This is the only link between
// blocks; if/for ops will reuse the same pattern.
struct BlockOpImpl {
  const Block* block = nullptr;
};

}  // namespace kvm::ir

KVM_MARK_MEMBER(::kvm::ir::OperationImpl, ::kvm::ir::BlockOpImpl);

namespace kvm::ir {

class Arena;

// Block: one level of topology. Owns no storage (the arena does); it organizes
// the nodes that belong to this level -- their program order, the block's
// argument/output interface, and the edges among them. It is also the builder:
// MakeOperation allocates via the arena and wires the result in one step.
class Block {
 public:
  const std::string& name() const { return name_; }

  // --- interface (the block's input/output value lists) ---

  // arguments: this block's level inputs. Their def() is nullptr (Null).
  const std::vector<ValueNode*>& arguments() const { return arguments_; }
  // outputs: the values this block exposes (produced by ops inside it). This is
  // an intrinsic property of the block's topology, symmetric to arguments --
  // there is no separate `return` terminator op.
  const std::vector<ValueNode*>& outputs() const { return outputs_; }

  // operations: the ops belonging to this block, in program order.
  const std::vector<OpNode*>& operations() const { return operations_; }

  // --- building ---

  // Add a block argument (a level input value). Returns its node (def==null).
  ValueNode* AddArgument(Value value);

  // Create an operation in this block: allocate the op node (arena) and wire it
  // (operands, results, block membership, use lists) in one atomic step. The
  // op is appended in program order. `results` are output payloads; each is
  // allocated as a value node whose def is this op. Returns the op node.
  OpNode* MakeOperation(Operation op, std::vector<ValueNode*> operands,
                        std::vector<Value> results);

  // Declare the block's exposed outputs.
  void SetOutputs(std::vector<ValueNode*> outputs) {
    outputs_ = std::move(outputs);
  }

  // --- transforms (the atomic write path for mutation) ---

  // Repoint operand `index` of `op` to `new_value`, updating the operand edge
  // and both values' use lists together.
  void SetOperand(OpNode* op, std::size_t index, ValueNode* new_value);

  // Replace every use of `old_value` with `new_value` across this block.
  // Returns the number of uses rewritten.
  std::size_t ReplaceAllUses(ValueNode* old_value, ValueNode* new_value);

  // Remove `op` from the block: detach it from its operands' use lists and drop
  // it from program order. Its result values become defless (callers must have
  // already rerouted their uses, e.g. via ReplaceAllUses). No storage is freed
  // (the arena outlives all nodes).
  void EraseOp(OpNode* op);

  // Constructed only via Arena::NewBlock (public because the arena's deque does
  // the construction, not a friend). Do not call directly.
  Block(Arena* arena, std::string name)
      : arena_(arena), name_(std::move(name)) {}

 private:
  // Record / drop `op` as a user of `value` at operand slot `index`.
  static void AddUse(ValueNode* value, OpNode* op, std::size_t index);
  static void RemoveUse(ValueNode* value, OpNode* op, std::size_t index);

  Arena* arena_;
  std::string name_;
  std::vector<ValueNode*> arguments_;
  std::vector<ValueNode*> outputs_;
  std::vector<OpNode*> operations_;
};

// Arena: the one owner of node storage for a whole graph. It only allocates and
// holds the real ValueNode / OpNode / Block objects (stable addresses via
// deque) and hands out pointers. It knows nothing about topology -- edges are
// Block's job. Every block in the graph shares this one arena. (Defined after
// Block so its deque<Block> sees the complete type.)
class Arena {
 public:
  Arena() = default;
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  // Allocate a value node holding `value`. If its name is empty, a default
  // label (v0, v1, ...) is filled in. Returns a free node (no edges yet).
  ValueNode* NewValue(Value value) {
    if (value.name.empty()) value.name = "v" + std::to_string(value_id_++);
    values_.emplace_back();
    values_.back().value_ = std::move(value);
    return &values_.back();
  }

  // Allocate an operation node holding `op`. Returns a free node (no edges).
  OpNode* NewOp(Operation op) {
    ops_.emplace_back();
    ops_.back().op_ = std::move(op);
    return &ops_.back();
  }

  // Allocate a block bound to this arena. If its name is empty, a default label
  // (block0, block1, ...) is filled in. Returns an empty block (no ops/args).
  Block* NewBlock(std::string name) {
    if (name.empty()) name = "block" + std::to_string(block_id_++);
    blocks_.emplace_back(this, std::move(name));
    return &blocks_.back();
  }

 private:
  std::deque<ValueNode> values_;
  std::deque<OpNode> ops_;
  std::deque<Block> blocks_;
  std::size_t value_id_ = 0;
  std::size_t block_id_ = 0;
};

// Graph :: (root: Block, config). The aggregate the API passes around: it holds
// the arena (owning all nodes) and the root block. Create the root once with
// MakeRoot, then build through it; nested blocks come from arena().NewBlock.
class Graph {
 public:
  Graph() = default;
  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  Arena& arena() { return arena_; }

  // Create the root block (call once). Returns it; also reachable via root().
  Block* MakeRoot(std::string name = "") {
    root_ = arena_.NewBlock(std::move(name));
    return root_;
  }

  // Designate an already-allocated arena block as the root (used when blocks
  // are built bottom-up, e.g. the parser allocates then sets the outermost).
  void SetRoot(Block* root) { root_ = root; }

  Block* root() { return root_; }
  const Block* root() const { return root_; }

  GraphConfig& config() { return config_; }
  const GraphConfig& config() const { return config_; }

 private:
  Arena arena_;
  Block* root_ = nullptr;
  GraphConfig config_;
};

}  // namespace kvm::ir
