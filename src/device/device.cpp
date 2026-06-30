#include "device/device.h"

#include <glog/logging.h>

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "dialect/kernel/values.h"
#include "dialect/task/dialect.h"
#include "dialect/task/values.h"
#include "value.h"

namespace kvm::device {
namespace {

namespace kernel = ir::kernel;
namespace task = ir::task;
using ir::AnyOf;
using ir::Block;
using ir::OperationImpl;
using ir::Operator;
using ir::OpNode;
using ir::Type;
using ir::Value;
using ir::ValueNode;

const kernel::TensorImpl* AsKernelTensor(const ValueNode* v) {
  return v ? v->value().impl.as<kernel::TensorImpl>() : nullptr;
}
const task::TensorImpl* AsTaskTensor(const ValueNode* v) {
  return v ? v->value().impl.as<task::TensorImpl>() : nullptr;
}

// "x_w_b_comm_moe_dispatch": the operand names joined, then the op name with
// its '.' flattened. The subgraph's inner values hang off this base so their
// lineage (which op, from which operands) is readable in the dump.
std::string NameBase(std::span<const std::string> input_names,
                     const std::string& op_name) {
  std::string base;
  for (const std::string& n : input_names) {
    base += n;
    base += '_';
  }
  for (char c : op_name) base += (c == '.') ? '_' : c;
  base += '_';  // separates the op-name base from each value's suffix
  return base;
}

// The emit context for one kernel op's expansion: it knows the block to append
// into, the name base, and the task operator table. Each Emit creates one task
// op and returns its single result value node.
struct Lowerer {
  const ir::serial::Registry& ops;
  Block* block;
  std::string base;

  Type T() const { return {"tensor", "task"}; }

  Operator Op(const std::string& key) const {
    auto op = ops.GetOperator(key);
    CHECK(op) << "decomp: task op '" << key << "' is not registered";
    return *op;
  }

  // Emit a single-output task op. `suffix` names the result (base_suffix);
  // shape/dtype/loc define the result tensor.
  ValueNode* Emit(const std::string& key, AnyOf<OperationImpl> impl,
                  std::vector<ValueNode*> ins, const std::string& suffix,
                  std::vector<int> shape, task::Dtype dtype,
                  task::Location loc) {
    Value out{base + suffix, T(),
              task::TensorImpl{std::move(shape), {}, dtype, loc}};
    OpNode* op = block->MakeOperation(ir::Operation{Op(key), std::move(impl)},
                                      std::move(ins), {std::move(out)});
    return op->results()[0];
  }
};

// gemm: D = A·B + C. At the task level there is no tiling yet, so shapes stay
// the kernel shapes; decomp only places operands and picks the instructions.
// A,B are staged into shared via TMA; the accumulator C is loaded to register;
// the wgmma-style mma_acc produces D in register; D is stored back to global.
std::vector<ValueNode*> LowerGemm(Lowerer& lo, std::span<ValueNode* const> ins,
                                  const ValueNode* kernel_out) {
  const task::TensorImpl* a = AsTaskTensor(ins[0]);
  const task::TensorImpl* b = AsTaskTensor(ins[1]);
  const task::TensorImpl* c = AsTaskTensor(ins[2]);
  const kernel::TensorImpl* d = AsKernelTensor(kernel_out);
  CHECK(a && b && c) << "decomp gemm: operands are not task tensors";
  CHECK(d) << "decomp gemm: result is not a kernel tensor";

  ValueNode* a_smem = lo.Emit("task.tma.gmem_to_smem", {}, {ins[0]}, "a_smem",
                              a->shape, a->dtype, task::Location::kShared);
  ValueNode* b_smem = lo.Emit("task.tma.gmem_to_smem", {}, {ins[1]}, "b_smem",
                              b->shape, b->dtype, task::Location::kShared);
  ValueNode* c_reg = lo.Emit("task.lsu.gmem_to_reg", {}, {ins[2]}, "c_reg",
                             c->shape, c->dtype, task::Location::kRegister);
  ValueNode* d_reg =
      lo.Emit("task.tc.mma_acc", {}, {a_smem, b_smem, c_reg}, "mma", d->shape,
              d->dtype, task::Location::kRegister);
  ValueNode* d_out = lo.Emit("task.lsu.reg_to_gmem", {}, {d_reg}, "out",
                             d->shape, d->dtype, task::Location::kGlobal);
  return {d_out};
}

// A network collective stays a single task sync primitive (like warp.reduce):
// it is not decomposed further here. The op's rank constants are carried over
// by copying the kernel op's impl into the task op of the same name.
std::vector<ValueNode*> LowerCollective(Lowerer& lo, const OpNode* kernel_op,
                                        std::span<ValueNode* const> ins,
                                        const ValueNode* kernel_out) {
  const kernel::TensorImpl* out = AsKernelTensor(kernel_out);
  CHECK(out) << "decomp collective: result is not a kernel tensor";
  std::string key = "task." + kernel_op->op().op.name;
  std::vector<ValueNode*> in_vec(ins.begin(), ins.end());
  ValueNode* v = lo.Emit(key, kernel_op->op().impl, std::move(in_vec), "out",
                         out->shape, out->dtype, task::Location::kGlobal);
  return {v};
}

}  // namespace

Device::Device(const ir::GraphConfig& config) : config_(config) {
  task::RegisterTaskDialect(ops_);
}

std::vector<ir::ValueNode*> Device::Decomp(
    const ir::OpNode* kernel_op, std::span<ir::ValueNode* const> inputs,
    std::span<const std::string> input_names,
    std::span<const ir::ValueNode* const> kernel_outputs,
    ir::Block* block) const {
  const std::string& name = kernel_op->op().op.name;
  Lowerer lo{ops_, block, NameBase(input_names, name)};

  // The MoE module: dispatch -> gemm -> combine. Only these are wired up; any
  // other kernel op CHECK-fails (not done != legal -- fail rather than drop).
  if (name == "gemm") {
    CHECK_EQ(kernel_outputs.size(), 1u) << "decomp gemm: expected 1 output";
    return LowerGemm(lo, inputs, kernel_outputs[0]);
  }
  if (name == "comm.moe_dispatch" || name == "comm.moe_combine") {
    CHECK_EQ(kernel_outputs.size(), 1u)
        << "decomp " << name << ": expected 1 output";
    return LowerCollective(lo, kernel_op, inputs, kernel_outputs[0]);
  }

  CHECK(false) << "decomp: no lowering for kernel op '" << name << "'";
  return {};  // unreachable
}

}  // namespace kvm::device
