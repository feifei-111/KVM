// Tests for the textual serializer.
#include <string>

#include "attr.h"
#include "graph.h"
#include "serialization/builtin_codecs.h"
#include "serialization/serializer.h"
#include "test_harness.h"

using namespace kvm::ir;

namespace {
Type Tensor() { return {"tensor", "hlo"}; }
Operator Matmul() {
  return {"matmul", "hlo", {Tensor(), Tensor()}, {Tensor()}};
}

serial::Registry MakeRegistry() {
  serial::Registry r;
  serial::RegisterBuiltins(r);
  return r;
}
}  // namespace

KVM_TEST(serialize_leaf_impl_and_operands) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* a = root->AddArgument(Value{"a", Tensor(), {}});
  ValueNode* b = root->AddArgument(Value{"b", Tensor(), {}});
  root->MakeOperation(Operation{Matmul(), {}}, {a, b},
                      {Value{"c", Tensor(), {}}});

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg);

  // Spot-check the shape (operations have no name; %name: type = op(...)).
  KVM_CHECK(text.find("%c: hlo.tensor = hlo.matmul(%a, %b)") !=
            std::string::npos);
  KVM_CHECK(text.find("graph {") != std::string::npos);
}

KVM_TEST(serialize_const_impl_in_angle_brackets) {
  Graph g;
  Block* root = g.MakeRoot();
  Operator constOp{"const", "builtin", {}, {Type{"int", "builtin"}}};
  root->MakeOperation(Operation{constOp, {}}, {},
                      {Value{"k", {"int", "builtin"}, ConstIntImpl{42}}});

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg);
  KVM_CHECK(text.find("%k: builtin.int<42>") != std::string::npos);
}

KVM_TEST(empty_impl_and_attr_are_omitted) {
  Graph g;
  g.MakeRoot();  // empty root block, no args/ops

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg);
  // a tensor value with empty impl prints no <...> and no {...}
  KVM_CHECK(text.find("hlo.tensor<") == std::string::npos);
  KVM_CHECK(text.find("{") !=
            std::string::npos);  // only the block/graph braces
}

KVM_TEST(attr_rendered_as_json_like) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* a = root->AddArgument(Value{"a", Tensor(), {}});

  AttrMap attrs;
  attrs.Set(a, "rank", 3);

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg, &attrs);
  KVM_CHECK(text.find("{rank = 3}") != std::string::npos);
}

KVM_TEST(nested_block_op_renders_recursively) {
  Graph g;
  Block* inner = g.arena().NewBlock("inner");
  ValueNode* ia = inner->AddArgument(Value{"ia", Tensor(), {}});
  Operator neg{"neg", "hlo", {Tensor()}, {Tensor()}};
  OpNode* iop = inner->MakeOperation(Operation{neg, {}}, {ia},
                                     {Value{"ib", Tensor(), {}}});
  inner->SetOutputs({iop->results()[0]});

  Block* root = g.MakeRoot();
  Operator blockop{"block", "builtin", {}, {}};
  root->MakeOperation(Operation{blockop, BlockOpImpl{inner}}, {}, {});

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg);
  // inner op appears inside the block-op's <...>
  KVM_CHECK(text.find("builtin.block<") != std::string::npos);
  KVM_CHECK(text.find("hlo.neg(%ia)") != std::string::npos);
}

KVM_RUN_ALL()
