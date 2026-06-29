// Tests for the kernel-graph dialect: build with its ops/impls, serialize,
// parse back through the registered operators, and check round-trip.
#include <string>

#include "dialect/kernel/dialect.h"
#include "dialect/kernel/ops.h"
#include "dialect/kernel/values.h"
#include "graph.h"
#include "serialization/parser.h"
#include "serialization/serializer.h"
#include "test_harness.h"

using namespace kvm::ir;
using namespace kvm::ir::kernel;

namespace {
Type T() { return {"tensor", "kernel"}; }
TensorImpl F16(std::vector<int> shape) {
  return TensorImpl{std::move(shape), {}, Dtype::kF16};
}
serial::Registry MakeRegistry() {
  serial::Registry r;
  RegisterKernelDialect(r);
  return r;
}
}  // namespace

KVM_TEST(tensor_impl_round_trip) {
  TensorImpl t = F16({8, 4096});
  std::string s = t.Serialize();
  TensorImpl back = TensorImpl::Deserialize(s);
  KVM_CHECK(back.dtype == Dtype::kF16);
  KVM_CHECK(back.shape.size() == 2 && back.shape[0] == 8 &&
            back.shape[1] == 4096);
}

KVM_TEST(attn_meta_round_trip) {
  AttnMetaImpl m;
  m.q_batch = {{0, 4, 8}, {12, 28}};
  m.kv_batch.desc = {{0, 16, 48}, {}};
  m.kv_batch.block_table = {{3}, {7, 8}};
  m.mask = AttnMask::kCausal;

  AttnMetaImpl back = AttnMetaImpl::Deserialize(m.Serialize());
  KVM_CHECK(back.q_batch.offsets.size() == 3 && back.q_batch.offsets[1] == 4);
  KVM_CHECK(back.kv_batch.block_table.size() == 2);
  KVM_CHECK(back.kv_batch.block_table[1].size() == 2 &&
            back.kv_batch.block_table[1][1] == 8);
  KVM_CHECK(back.mask == AttnMask::kCausal);
}

KVM_TEST(graph_round_trip_with_dialect_ops) {
  auto reg = MakeRegistry();

  Graph g;
  const Value* x = g.MakeInput("x", T(), F16({8, 4096}));
  const Value* w = g.MakeInput("w", T(), F16({4096, 4096}));
  Block* root = g.MakeBlock({x, w}, "entry");

  Operator matmul = *reg.GetOperator("kernel.matmul");
  const Operation* mm = g.MakeOperation(root, matmul, {}, {x, w},
                                        {Value{"y", T(), F16({8, 4096})}});
  const Value* y = g.GetOutputs(mm)[0];

  Operator a2a = *reg.GetOperator("kernel.comm.all_to_all");
  g.MakeOperation(root, a2a, AllToAllOp{{0, 1, 2, 3}}, {y},
                  {Value{"z", T(), F16({8, 4096})}});
  g.SetMain(root);

  std::string text1 = serial::Serialize(g, reg);
  Graph g2;
  serial::Parse(text1, reg, g2);
  std::string text2 = serial::Serialize(g2, reg);
  KVM_CHECK(text1 == text2);

  // the all_to_all op's impl carries its ranks through the round trip
  const Block* m = g2.main();
  const Operation* last = m->operations.back();
  const AllToAllOp* impl = last->impl.as<AllToAllOp>();
  KVM_CHECK(impl != nullptr && impl->ranks.size() == 4 && impl->ranks[3] == 3);
}

KVM_RUN_ALL()
