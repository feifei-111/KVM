// Tests for the textual serializer.
#include <string>

#include "attr.h"
#include "builtin.h"
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
  Block* root = g.MakeBlock();
  const Value* a = g.MakeInput("a", Tensor());
  const Value* b = g.MakeInput("b", Tensor());
  g.MakeOperation(root, Matmul(), {}, {a, b}, {Value{"c", Tensor(), {}}});
  g.SetMain(root);

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg);

  // Spot-check the shape (operations have no name; %name: type = op(...)).
  KVM_CHECK(text.find("%c: hlo.tensor = hlo.matmul(%a, %b)") !=
            std::string::npos);
  KVM_CHECK(text.find("graph {") != std::string::npos);
}

KVM_TEST(serialize_const_impl_in_angle_brackets) {
  Graph g;
  Block* root = g.MakeBlock();
  Operator constOp{"const", "builtin", {}, {Type{"int", "builtin"}}};
  g.MakeOperation(root, constOp, {}, {},
                  {Value{"k", {"int", "builtin"}, ConstIntImpl{42}}});
  g.SetMain(root);

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg);
  KVM_CHECK(text.find("%k: builtin.int<42>") != std::string::npos);
}

KVM_TEST(empty_impl_and_attr_are_omitted) {
  Graph g;
  Block* root = g.MakeBlock();
  const Value* a = g.MakeInput("a", Tensor());  // no impl, no attr
  g.SetMain(root);
  (void)a;

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg);
  // a tensor value with empty impl prints no <...> and no {...}
  KVM_CHECK(text.find("hlo.tensor<") == std::string::npos);
  KVM_CHECK(text.find("{") !=
            std::string::npos);  // only the block/graph braces
}

KVM_TEST(attr_rendered_as_json_like) {
  Graph g;
  const Value* a = g.MakeInput("a", Tensor());
  Block* root = g.MakeBlock({a});  // a is a block input, so it gets printed
  g.SetMain(root);

  AttrMap attrs;
  attrs.Set(a, "rank", 3);

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg, &attrs);
  KVM_CHECK(text.find("{rank = 3}") != std::string::npos);
}

KVM_TEST(nested_block_op_renders_recursively) {
  Graph g;
  const Value* ia = g.MakeInput("ia", Tensor());
  Block* inner = g.MakeBlock({ia});
  Operator neg{"neg", "hlo", {Tensor()}, {Tensor()}};
  const Operation* iop =
      g.MakeOperation(inner, neg, {}, {ia}, {Value{"ib", Tensor(), {}}});
  g.SetBlockOutputs(inner, {g.GetOutputs(iop)[0]});

  Block* root = g.MakeBlock();
  Operator blockop{"block", "builtin", {}, {}};
  g.MakeOperation(root, blockop, BlockOpImpl{inner}, {}, {});
  g.SetMain(root);

  auto reg = MakeRegistry();
  std::string text = serial::Serialize(g, reg);
  // inner op appears inside the block-op's <...>
  KVM_CHECK(text.find("builtin.block<") != std::string::npos);
  KVM_CHECK(text.find("hlo.neg(%ia)") != std::string::npos);
}

KVM_RUN_ALL()
