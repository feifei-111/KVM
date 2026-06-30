// Round-trip tests: serialize -> parse -> serialize must be stable.
#include <cstdint>
#include <string>

#include "attr.h"
#include "builtin.h"
#include "graph.h"
#include "serialization/builtin_codecs.h"
#include "serialization/parser.h"
#include "serialization/serializer.h"
#include "test_harness.h"

using namespace kvm::ir;

namespace {
Type Tensor() { return {"tensor", "hlo"}; }
Operator Matmul() {
  return {"matmul", "hlo", {Tensor(), Tensor()}, {Tensor()}};
}
Operator ConstOp() {
  return {"const", "builtin", {}, {Type{"int", "builtin"}}};
}

serial::Registry MakeRegistry() {
  serial::Registry r;
  serial::RegisterBuiltins(r);
  // operators the parser must look up by "dialect.name"
  r.RegisterOperator("hlo.matmul", Matmul());
  r.RegisterOperator("builtin.const", ConstOp());
  return r;
}
}  // namespace

KVM_TEST(round_trip_simple_graph) {
  Graph g;
  Block* root = g.MakeRoot("entry");
  ValueNode* a = root->AddArgument(Value{"a", Tensor(), {}});
  ValueNode* b = root->AddArgument(Value{"b", Tensor(), {}});
  OpNode* op = root->MakeOperation(Operation{Matmul(), {}}, {a, b},
                                   {Value{"c", Tensor(), {}}});
  root->SetOutputs({op->results()[0]});
  g.config().dist.all_ranks = {0, 1, 2, 3};

  auto reg = MakeRegistry();
  std::string text1 = serial::Serialize(g, reg);

  Graph g2;
  serial::Parse(text1, reg, g2);
  std::string text2 = serial::Serialize(g2, reg);

  KVM_CHECK(text1 == text2);
}

KVM_TEST(round_trip_preserves_const_impl) {
  Graph g;
  Block* root = g.MakeRoot("entry");
  root->MakeOperation(Operation{ConstOp(), {}}, {},
                      {Value{"k", {"int", "builtin"}, ConstIntImpl{42}}});

  auto reg = MakeRegistry();
  std::string text1 = serial::Serialize(g, reg);

  Graph g2;
  serial::Parse(text1, reg, g2);

  // the parsed value carries a ConstIntImpl{42} again
  const Block* m = g2.root();
  const OpNode* op = m->operations()[0];
  const Value& k = op->results()[0]->value();
  const ConstIntImpl* ci = k.impl.as<ConstIntImpl>();
  KVM_CHECK(ci != nullptr && ci->value == 42);
  KVM_CHECK(serial::Serialize(g2, reg) == text1);
}

KVM_TEST(round_trip_attrs) {
  Graph g;
  Block* root = g.MakeRoot("entry");
  ValueNode* a = root->AddArgument(Value{"a", Tensor(), {}});

  AttrMap attrs;
  attrs.Set(a, "rank", std::int64_t{3});

  auto reg = MakeRegistry();
  std::string text1 = serial::Serialize(g, reg, &attrs);

  Graph g2;
  AttrMap attrs2 = serial::Parse(text1, reg, g2);
  std::string text2 = serial::Serialize(g2, reg, &attrs2);

  KVM_CHECK(text1 == text2);
  KVM_CHECK(text2.find("{rank = 3}") != std::string::npos);
}

KVM_TEST(round_trip_nested_block_op) {
  Graph g;
  Block* inner = g.arena().NewBlock("inner");
  ValueNode* ia = inner->AddArgument(Value{"ia", Tensor(), {}});
  Operator neg{"neg", "hlo", {Tensor()}, {Tensor()}};
  OpNode* iop = inner->MakeOperation(Operation{neg, {}}, {ia},
                                     {Value{"ib", Tensor(), {}}});
  inner->SetOutputs({iop->results()[0]});

  Block* root = g.MakeRoot("entry");
  Operator blockop{"block", "builtin", {}, {}};
  root->MakeOperation(Operation{blockop, BlockOpImpl{inner}}, {}, {});

  auto reg = MakeRegistry();
  reg.RegisterOperator("hlo.neg", neg);
  reg.RegisterOperator("builtin.block", blockop);
  std::string text1 = serial::Serialize(g, reg);

  Graph g2;
  serial::Parse(text1, reg, g2);
  std::string text2 = serial::Serialize(g2, reg);

  KVM_CHECK(text1 == text2);
}

KVM_RUN_ALL()
