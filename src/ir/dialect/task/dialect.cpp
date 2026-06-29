#include "dialect/task/dialect.h"

#include <string>
#include <vector>

#include "dialect/kernel/ops.h"
#include "dialect/task/ops.h"
#include "dialect/task/values.h"
#include "value.h"

namespace kvm::ir::task {
namespace {

constexpr const char* kDialect = "task";

Type Tensor() { return {"tensor", kDialect}; }

Operator Op(std::string name, std::vector<Type> ins, std::vector<Type> outs) {
  return Operator{std::move(name), kDialect, std::move(ins), std::move(outs)};
}

void RegisterOperators(serial::Registry& r) {
  const Type t = Tensor();

  auto reg = [&](const std::string& name, std::vector<Type> ins,
                 std::vector<Type> outs) {
    r.RegisterOperator(std::string(kDialect) + "." + name,
                       Op(name, std::move(ins), std::move(outs)));
  };

  // op names are task.<unit>.<fn>, the unit being the hardware component.
  // TC (Tensor Core / wgmma). reg/smem operand placement is on the value's loc.
  reg("tc.mma", {t, t}, {t});         // D = A·B
  reg("tc.mma_acc", {t, t, t}, {t});  // D = A·B + C  (kernel.gemm)

  // ALU (CUDA core, elementwise). No div: a/b == sfu.rcp(b) then alu.mul.
  for (const char* n : {"add", "sub", "mul", "max", "min"})
    reg(std::string("alu.") + n, {t, t}, {t});
  for (const char* n : {"neg", "abs"}) reg(std::string("alu.") + n, {t}, {t});
  reg("alu.select", {t, t, t}, {t});  // cond ? a : b  (mask)

  for (const char* n :
       {"cmp.eq", "cmp.ne", "cmp.lt", "cmp.le", "cmp.gt", "cmp.ge"})
    reg(std::string("alu.") + n, {t, t}, {t});

  for (const char* n : {"cast.f16", "cast.f32", "cast.bf16", "cast.i32"})
    reg(std::string("alu.") + n, {t}, {t});

  // SFU (special functions). exp/log are ex2/lg2 + a scale at lowering.
  for (const char* n : {"rcp", "rsqrt", "sqrt", "exp", "log", "sin", "cos"})
    reg(std::string("sfu.") + n, {t}, {t});

  // data movement: each direction is a distinct hardware instruction, so it is
  // its own op (no CopyOp needed). TMA moves global<->shared; LSU does the
  // rest.
  reg("tma.gmem_to_smem", {t}, {t});  // load  (cp.async.bulk.tensor)
  reg("tma.smem_to_gmem", {t}, {t});  // store
  reg("lsu.smem_to_reg", {t}, {t});   // ldmatrix
  reg("lsu.reg_to_smem", {t}, {t});   // stmatrix
  reg("lsu.gmem_to_reg", {t}, {t});   // ld.global
  reg("lsu.reg_to_gmem", {t}, {t});   // st.global

  // warp reduction (semantic op; axis on ReduceOp). broadcast undoes it.
  reg("warp.reduce.sum", {t}, {t});
  reg("warp.reduce.max", {t}, {t});
  reg("warp.broadcast", {t}, {t});

  // network / collectives (rank info on the OperationImpl, reused from kernel).
  reg("comm.send", {t}, {t});
  reg("comm.recv", {t}, {t});
  reg("comm.broadcast", {t}, {t});
  reg("comm.all_to_all", {t}, {t});
  reg("comm.all_reduce.sum", {t}, {t});
  reg("comm.all_reduce.max", {t}, {t});
  reg("comm.all_gather", {t}, {t});
  reg("comm.moe_dispatch", {t, t}, {t});
  reg("comm.moe_combine", {t, t}, {t});
}

void RegisterImplCodecs(serial::Registry& r) {
  // value impls
  r.RegisterValueImpl<TensorImpl>("task.tensor");

  // operation impls
  r.RegisterOperationImpl<ReduceOp>("task.warp.reduce.sum");
  r.RegisterOperationImpl<ReduceOp>("task.warp.reduce.max");
  r.RegisterOperationImpl<BroadcastOp>("task.warp.broadcast");

  // collectives reuse the kernel dialect's dist OperationImpls, keyed by the
  // task.comm.* op names.
  r.RegisterOperationImpl<kernel::SendOp>("task.comm.send");
  r.RegisterOperationImpl<kernel::RecvOp>("task.comm.recv");
  r.RegisterOperationImpl<kernel::BroadcastOp>("task.comm.broadcast");
  r.RegisterOperationImpl<kernel::AllToAllOp>("task.comm.all_to_all");
  r.RegisterOperationImpl<kernel::AllReduceSum>("task.comm.all_reduce.sum");
  r.RegisterOperationImpl<kernel::AllReduceMax>("task.comm.all_reduce.max");
  r.RegisterOperationImpl<kernel::AllGather>("task.comm.all_gather");
  r.RegisterOperationImpl<kernel::MoeDispatch>("task.comm.moe_dispatch");
  r.RegisterOperationImpl<kernel::MoeCombine>("task.comm.moe_combine");
}

}  // namespace

void RegisterTaskDialect(serial::Registry& r) {
  RegisterOperators(r);
  RegisterImplCodecs(r);
}

}  // namespace kvm::ir::task
