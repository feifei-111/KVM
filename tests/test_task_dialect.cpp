// Tests for the task-graph dialect: build with its ops/impls, serialize, parse
// back through the registered operators, and check round-trip.
#include <string>

#include "dialect/task/dialect.h"
#include "dialect/task/ops.h"
#include "dialect/task/values.h"
#include "graph.h"
#include "serialization/parser.h"
#include "serialization/serializer.h"
#include "test_harness.h"

using namespace kvm::ir;
using namespace kvm::ir::task;

namespace {
Type T() { return {"tensor", "task"}; }
TensorImpl F16(std::vector<int> shape, Location loc = Location::kGlobal) {
  return TensorImpl{std::move(shape), {}, Dtype::kF16, loc};
}
serial::Registry MakeRegistry() {
  serial::Registry r;
  RegisterTaskDialect(r);
  return r;
}
}  // namespace

KVM_TEST(tensor_impl_round_trip_with_loc) {
  TensorImpl t = F16({8, 128}, Location::kRegister);
  TensorImpl back = TensorImpl::Deserialize(t.Serialize());
  KVM_CHECK(back.dtype == Dtype::kF16);
  KVM_CHECK(back.loc == Location::kRegister);
  KVM_CHECK(back.shape.size() == 2 && back.shape[0] == 8 &&
            back.shape[1] == 128);
}

KVM_TEST(copy_ops_registered_by_direction) {
  auto reg = MakeRegistry();
  // data movement is named by direction; no CopyOp impl. Each direction is its
  // own registered operator.
  for (const char* name : {"task.tma.gmem_to_smem", "task.tma.smem_to_gmem",
                           "task.lsu.smem_to_reg", "task.lsu.reg_to_smem",
                           "task.lsu.gmem_to_reg", "task.lsu.reg_to_gmem"}) {
    KVM_CHECK(reg.GetOperator(name).has_value());
  }
}

KVM_TEST(reduce_op_round_trip) {
  ReduceOp r{1};
  KVM_CHECK(ReduceOp::Deserialize(r.Serialize()).axis == 1);
  BroadcastOp b{2};
  KVM_CHECK(BroadcastOp::Deserialize(b.Serialize()).axis == 2);
}

KVM_TEST(graph_round_trip_with_task_ops) {
  auto reg = MakeRegistry();

  Graph g;
  const Value* x = g.MakeInput("x", T(), F16({8, 128}, Location::kShared));
  const Value* w = g.MakeInput("w", T(), F16({128, 128}, Location::kShared));
  Block* root = g.MakeBlock({x, w}, "entry");

  // mma into a register accumulator
  Operator mma = *reg.GetOperator("task.tc.mma");
  const Operation* mm =
      g.MakeOperation(root, mma, {}, {x, w},
                      {Value{"y", T(), F16({8, 128}, Location::kRegister)}});
  const Value* y = g.GetOutputs(mm)[0];

  // a reduce carrying its axis on the OperationImpl
  Operator rsum = *reg.GetOperator("task.warp.reduce.sum");
  g.MakeOperation(root, rsum, ReduceOp{1}, {y},
                  {Value{"s", T(), F16({8, 1}, Location::kRegister)}});
  g.SetMain(root);

  std::string text1 = serial::Serialize(g, reg);
  Graph g2;
  serial::Parse(text1, reg, g2);
  std::string text2 = serial::Serialize(g2, reg);
  KVM_CHECK(text1 == text2);

  // the reduce op's impl carries its axis through the round trip
  const Block* m = g2.main();
  const Operation* last = m->operations.back();
  const ReduceOp* impl = last->impl.as<ReduceOp>();
  KVM_CHECK(impl != nullptr && impl->axis == 1);
}

KVM_RUN_ALL()
