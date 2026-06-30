// Tests for the topology layer: arena/block building, edges, nesting,
// transforms. Edges live on nodes (def/uses/operands/results); Block is the
// builder and the single writer of those edges.
#include <string>

#include "graph.h"
#include "test_harness.h"

using namespace kvm::ir;

namespace {
Type T() { return {"tensor", "hlo"}; }
Operator Add() { return {"add", "hlo", {T(), T()}, {T()}}; }
Operation AddOp() { return Operation{Add(), {}}; }
}  // namespace

KVM_TEST(edges_wire_on_build) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* a = root->AddArgument(Value{"a", T(), {}});
  ValueNode* b = root->AddArgument(Value{"b", T(), {}});
  OpNode* op = root->MakeOperation(AddOp(), {a, b}, {Value{"c", T(), {}}});
  ValueNode* c = op->results()[0];

  KVM_CHECK(c->def() == op);       // output's def is the op
  KVM_CHECK(a->def() == nullptr);  // block argument: Null
  KVM_CHECK(op->operands().size() == 2);
  KVM_CHECK(op->operands()[0] == a && op->operands()[1] == b);
  KVM_CHECK(a->uses().size() == 1 && a->uses()[0].user == op);
  KVM_CHECK(c->uses().empty());
}

KVM_TEST(repeated_operand_multiplicity) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* x = root->AddArgument(Value{"x", T(), {}});
  OpNode* op = root->MakeOperation(AddOp(), {x, x}, {Value{"y", T(), {}}});
  KVM_CHECK(x->uses().size() == 2);  // x used twice
  (void)op;
}

KVM_TEST(op_bound_to_block_in_program_order) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* x = root->AddArgument(Value{"x", T(), {}});
  OpNode* o1 = root->MakeOperation(AddOp(), {x, x}, {Value{"y", T(), {}}});
  ValueNode* y = o1->results()[0];
  OpNode* o2 = root->MakeOperation(AddOp(), {x, y}, {Value{"z", T(), {}}});

  KVM_CHECK(o1->block() == root && o2->block() == root);
  KVM_CHECK(root->operations().size() == 2);
  KVM_CHECK(root->operations()[0] == o1 && root->operations()[1] == o2);
}

KVM_TEST(nested_block_via_block_op_jia) {
  Graph g;
  // inner block built first
  Block* inner = g.arena().NewBlock("inner");
  ValueNode* ia = inner->AddArgument(Value{"ia", T(), {}});
  Operator neg{"neg", "hlo", {T()}, {T()}};
  OpNode* iop =
      inner->MakeOperation(Operation{neg, {}}, {ia}, {Value{"ib", T(), {}}});
  inner->SetOutputs({iop->results()[0]});

  // then referenced by a block-op in the outer (root) block
  Block* root = g.MakeRoot();
  Operator blockop{"block", "builtin", {}, {}};
  OpNode* bop =
      root->MakeOperation(Operation{blockop, BlockOpImpl{inner}}, {}, {});

  const BlockOpImpl* bi = bop->op().impl.as<BlockOpImpl>();
  KVM_CHECK(bi != nullptr && bi->block == inner);
  KVM_CHECK(iop->block() == inner);
  KVM_CHECK(g.root() == root);
  KVM_CHECK(inner->arguments().size() == 1 && inner->arguments()[0] == ia);
}

KVM_TEST(set_operand_moves_one_use) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* a = root->AddArgument(Value{"a", T(), {}});
  ValueNode* b = root->AddArgument(Value{"b", T(), {}});
  OpNode* op = root->MakeOperation(AddOp(), {a, b}, {Value{"c", T(), {}}});

  root->SetOperand(op, 0, b);  // a -> b at index 0
  KVM_CHECK(op->operands()[0] == b && op->operands()[1] == b);
  KVM_CHECK(a->uses().empty());
  KVM_CHECK(b->uses().size() == 2);
}

KVM_TEST(replace_all_uses) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* a = root->AddArgument(Value{"a", T(), {}});
  OpNode* o1 = root->MakeOperation(AddOp(), {a, a}, {Value{"c", T(), {}}});
  ValueNode* c = o1->results()[0];
  root->MakeOperation(AddOp(), {c, a}, {Value{"d", T(), {}}});

  ValueNode* e = root->AddArgument(Value{"e", T(), {}});
  std::size_t n = root->ReplaceAllUses(a, e);
  KVM_CHECK(n == 3);  // a used: o1 twice, o2 once
  KVM_CHECK(a->uses().empty());
  KVM_CHECK(e->uses().size() == 3);
}

KVM_TEST(erase_op_detaches_uses) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* a = root->AddArgument(Value{"a", T(), {}});
  ValueNode* b = root->AddArgument(Value{"b", T(), {}});
  OpNode* op = root->MakeOperation(AddOp(), {a, b}, {Value{"c", T(), {}}});

  root->EraseOp(op);
  KVM_CHECK(root->operations().empty());
  KVM_CHECK(a->uses().empty() && b->uses().empty());
}

KVM_RUN_ALL()
