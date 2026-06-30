// Tests for the kernel -> task decomp transform.
//
// Build a small MoE kernel graph (moe_dispatch -> gemm -> moe_combine), run
// Decomp with a Device, and check the produced task graph: each kernel op is
// replaced by its task subgraph, spliced flat, with lineage-carrying names.
#include <algorithm>
#include <string>
#include <vector>

#include "device/device.h"
#include "dialect/kernel/dialect.h"
#include "dialect/kernel/ops.h"
#include "dialect/kernel/values.h"
#include "dialect/task/dialect.h"
#include "dialect/task/values.h"
#include "graph.h"
#include "serialization/parser.h"
#include "serialization/registry.h"
#include "serialization/serializer.h"
#include "test_harness.h"
#include "transform/kernel_to_task/kernel_to_task.h"

using namespace kvm::ir;
using kvm::ir::kernel::RegisterKernelDialect;
using kvm::ir::task::RegisterTaskDialect;
using kvm::transform::TransformKernelToTask;

namespace {

Type KT() { return {"tensor", "kernel"}; }

kernel::TensorImpl KF16(std::vector<int> shape) {
  return kernel::TensorImpl{std::move(shape), {}, kernel::Dtype::kF16};
}

// Count task ops whose operator key matches `key`.
int CountOps(const Graph& g, const std::string& key) {
  int n = 0;
  for (const OpNode* op : g.root()->operations()) {
    const Operator& o = op->op().op;
    if ((o.dialect.empty() ? o.name : o.dialect + "." + o.name) == key) ++n;
  }
  return n;
}

bool HasValueNamed(const Graph& g, const std::string& name) {
  for (const OpNode* op : g.root()->operations())
    for (const ValueNode* v : op->results())
      if (v->value().name == name) return true;
  return false;
}

}  // namespace

// A MoE kernel block: %d = moe_dispatch(%x, %g); %y = gemm(%d, %w, %b);
// %out = moe_combine(%y, %g). Returns the kernel graph (built in `g`).
void BuildMoeKernel(Graph& g, serial::Registry& reg) {
  RegisterKernelDialect(reg);
  Block* root = g.MakeRoot("moe");
  ValueNode* x = root->AddArgument(Value{"x", KT(), KF16({8, 16})});
  ValueNode* gate = root->AddArgument(Value{"g", KT(), KF16({8, 4})});
  ValueNode* w = root->AddArgument(Value{"w", KT(), KF16({16, 16})});
  ValueNode* bias = root->AddArgument(Value{"b", KT(), KF16({8, 16})});

  Operator disp = *reg.GetOperator("kernel.comm.moe_dispatch");
  OpNode* d = root->MakeOperation(Operation{disp, kernel::MoeDispatch{{0, 1}}},
                                  {x, gate}, {Value{"d", KT(), KF16({8, 16})}});
  ValueNode* dv = d->results()[0];

  Operator gemm = *reg.GetOperator("kernel.gemm");
  OpNode* y = root->MakeOperation(Operation{gemm, {}}, {dv, w, bias},
                                  {Value{"y", KT(), KF16({8, 16})}});
  ValueNode* yv = y->results()[0];

  Operator comb = *reg.GetOperator("kernel.comm.moe_combine");
  OpNode* out =
      root->MakeOperation(Operation{comb, kernel::MoeCombine{{0, 1}}},
                          {yv, gate}, {Value{"out", KT(), KF16({8, 16})}});
  root->SetOutputs({out->results()[0]});
}

KVM_TEST(decomp_moe_expands_each_op) {
  Graph kernel;
  serial::Registry reg;
  BuildMoeKernel(kernel, reg);

  Graph task;
  TransformKernelToTask(kernel, task);

  // gemm expands into TMA loads + mma_acc + a store.
  KVM_CHECK(CountOps(task, "task.tma.gmem_to_smem") == 2);  // A, B
  KVM_CHECK(CountOps(task, "task.lsu.gmem_to_reg") == 1);   // C
  KVM_CHECK(CountOps(task, "task.tc.mma_acc") == 1);
  KVM_CHECK(CountOps(task, "task.lsu.reg_to_gmem") == 1);  // D
  // collectives stay single sync primitives.
  KVM_CHECK(CountOps(task, "task.comm.moe_dispatch") == 1);
  KVM_CHECK(CountOps(task, "task.comm.moe_combine") == 1);
}

KVM_TEST(decomp_names_carry_lineage) {
  Graph kernel;
  serial::Registry reg;
  BuildMoeKernel(kernel, reg);
  Graph task;
  TransformKernelToTask(kernel, task);

  // gemm operands are %d,%w,%b -> base "d_w_b_gemm"; the mma result is _mma.
  KVM_CHECK(HasValueNamed(task, "d_w_b_gemm_mma"));
  KVM_CHECK(HasValueNamed(task, "d_w_b_gemm_a_smem"));
  KVM_CHECK(HasValueNamed(task, "d_w_b_gemm_out"));
}

KVM_TEST(decomp_preserves_collective_ranks) {
  Graph kernel;
  serial::Registry reg;
  BuildMoeKernel(kernel, reg);
  Graph task;
  TransformKernelToTask(kernel, task);

  // moe_dispatch's rank set survives onto the task collective's impl.
  for (const OpNode* op : task.root()->operations()) {
    if (op->op().op.name == "comm.moe_dispatch") {
      const kernel::MoeDispatch* impl = op->op().impl.as<kernel::MoeDispatch>();
      KVM_CHECK(impl != nullptr && impl->ranks == std::vector<int>({0, 1}));
    }
  }
}

KVM_TEST(decomp_round_trips_through_text) {
  Graph kernel;
  serial::Registry kreg;
  BuildMoeKernel(kernel, kreg);
  Graph task;
  TransformKernelToTask(kernel, task);

  // The produced task graph serializes and parses back identically.
  serial::Registry treg;
  task::RegisterTaskDialect(treg);
  std::string text1 = serial::Serialize(task, treg);
  Graph task2;
  serial::Parse(text1, treg, task2);
  std::string text2 = serial::Serialize(task2, treg);
  KVM_CHECK(text1 == text2);
}

KVM_RUN_ALL()
