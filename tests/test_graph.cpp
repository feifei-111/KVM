// Tests for Graph: edges, blocks, nesting, transforms, consistency.
#include <string>

#include "builtin.h"
#include "graph.h"
#include "test_harness.h"

using namespace kvm::ir;

namespace {
Type T() { return {"tensor", "hlo"}; }
Operator Add() { return {"add", "hlo", {T(), T()}, {T()}}; }
}  // namespace

KVM_TEST(edges_wire_on_build) {
  Graph g;
  Block* root = g.MakeBlock();
  const Value* a = g.MakeInput("a", T());
  const Value* b = g.MakeInput("b", T());
  const Operation* op =
      g.MakeOperation(root, Add(), {}, {a, b}, {Value{"c", T(), {}}});
  const Value* c = g.GetOutputs(op)[0];

  KVM_CHECK(g.GetDef(c) == op);       // output's def is the op
  KVM_CHECK(g.GetDef(a) == nullptr);  // input: Null
  KVM_CHECK(g.GetInputs(op).size() == 2);
  KVM_CHECK(g.GetInputs(op)[0] == a && g.GetInputs(op)[1] == b);
  KVM_CHECK(g.GetUser(a).size() == 1 && g.GetUser(a)[0] == op);
  KVM_CHECK(g.GetUser(c).empty());
}

KVM_TEST(repeated_operand_multiplicity) {
  Graph g;
  Block* root = g.MakeBlock();
  const Value* x = g.MakeInput("x", T());
  const Operation* op =
      g.MakeOperation(root, Add(), {}, {x, x}, {Value{"y", T(), {}}});
  KVM_CHECK(g.GetUser(x).size() == 2);  // x used twice
  (void)op;
}

KVM_TEST(op_bound_to_block_in_program_order) {
  Graph g;
  Block* root = g.MakeBlock();
  const Value* x = g.MakeInput("x", T());
  const Operation* o1 =
      g.MakeOperation(root, Add(), {}, {x, x}, {Value{"y", T(), {}}});
  const Value* y = g.GetOutputs(o1)[0];
  const Operation* o2 =
      g.MakeOperation(root, Add(), {}, {x, y}, {Value{"z", T(), {}}});

  KVM_CHECK(g.GetBlock(o1) == root && g.GetBlock(o2) == root);
  KVM_CHECK(root->operations.size() == 2);
  KVM_CHECK(root->operations[0] == o1 && root->operations[1] == o2);
}

KVM_TEST(nested_block_via_block_op_jia) {
  Graph g;
  // inner block built first
  const Value* ia = g.MakeInput("ia", T());
  Block* inner = g.MakeBlock({ia});
  Operator neg{"neg", "hlo", {T()}, {T()}};
  const Operation* iop =
      g.MakeOperation(inner, neg, {}, {ia}, {Value{"ib", T(), {}}});
  g.SetBlockOutputs(inner, {g.GetOutputs(iop)[0]});

  // then referenced by a block-op in the outer block
  Block* root = g.MakeBlock();
  Operator blockop{"block", "builtin", {}, {}};
  const Operation* bop =
      g.MakeOperation(root, blockop, BlockOpImpl{inner}, {}, {});
  g.SetMain(root);

  const BlockOpImpl* bi = bop->impl.as<BlockOpImpl>();
  KVM_CHECK(bi != nullptr && bi->block == inner);
  KVM_CHECK(g.GetBlock(iop) == inner);
  KVM_CHECK(g.main() == root);
  KVM_CHECK(inner->inputs.size() == 1 && inner->inputs[0] == ia);
}

KVM_TEST(set_input_moves_one_use) {
  Graph g;
  Block* root = g.MakeBlock();
  const Value* a = g.MakeInput("a", T());
  const Value* b = g.MakeInput("b", T());
  const Operation* op =
      g.MakeOperation(root, Add(), {}, {a, b}, {Value{"c", T(), {}}});

  g.SetInput(op, 0, b);  // a -> b at index 0
  KVM_CHECK(g.GetInputs(op)[0] == b && g.GetInputs(op)[1] == b);
  KVM_CHECK(g.GetUser(a).empty());
  KVM_CHECK(g.GetUser(b).size() == 2);
  std::string err;
  KVM_CHECK(g.CheckConsistency(&err));
}

KVM_TEST(replace_all_uses) {
  Graph g;
  Block* root = g.MakeBlock();
  const Value* a = g.MakeInput("a", T());
  const Operation* o1 =
      g.MakeOperation(root, Add(), {}, {a, a}, {Value{"c", T(), {}}});
  const Value* c = g.GetOutputs(o1)[0];
  const Operation* o2 =
      g.MakeOperation(root, Add(), {}, {c, a}, {Value{"d", T(), {}}});
  (void)o2;

  const Value* e = g.MakeInput("e", T());
  std::size_t n = g.ReplaceAllUses(a, e);
  KVM_CHECK(n == 3);  // a used: o1 twice, o2 once
  KVM_CHECK(g.GetUser(a).empty());
  KVM_CHECK(g.GetUser(e).size() == 3);
  std::string err;
  KVM_CHECK(g.CheckConsistency(&err));
}

KVM_TEST(consistency_holds_after_build) {
  Graph g;
  Block* root = g.MakeBlock();
  const Value* a = g.MakeInput("a", T());
  const Value* b = g.MakeInput("b", T());
  g.MakeOperation(root, Add(), {}, {a, b}, {Value{"c", T(), {}}});
  std::string err;
  KVM_CHECK(g.CheckConsistency(&err));
  KVM_CHECK(err.empty());
}

KVM_RUN_ALL()
