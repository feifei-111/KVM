#include "dialect/tile/dialect.h"

#include <string>
#include <vector>

#include "dialect/kernel/ops.h"
#include "dialect/tile/ops.h"
#include "dialect/tile/values.h"
#include "value.h"

namespace kvm::ir::tile {
namespace {

constexpr const char* kDialect = "tile";

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

  // Same op set as the task dialect; only the prefix differs (task. -> tile.).
  // The tile-vs-task difference is a stricter signature (legal tile shapes),
  // checked against the device, not a different op list.

  // TC (Tensor Core / wgmma). reg/smem operand placement is on the value's loc.
  reg("tc.mma", {t, t}, {t});
  reg("tc.mma_acc", {t, t, t}, {t});

  // ALU (CUDA core, elementwise). No div: a/b == sfu.rcp(b) then alu.mul.
  for (const char* n : {"add", "sub", "mul", "max", "min"})
    reg(std::string("alu.") + n, {t, t}, {t});
  for (const char* n : {"neg", "abs"}) reg(std::string("alu.") + n, {t}, {t});
  reg("alu.select", {t, t, t}, {t});

  for (const char* n :
       {"cmp.eq", "cmp.ne", "cmp.lt", "cmp.le", "cmp.gt", "cmp.ge"})
    reg(std::string("alu.") + n, {t, t}, {t});

  for (const char* n : {"cast.f16", "cast.f32", "cast.bf16", "cast.i32"})
    reg(std::string("alu.") + n, {t}, {t});

  // SFU (special functions).
  for (const char* n : {"rcp", "rsqrt", "sqrt", "exp", "log", "sin", "cos"})
    reg(std::string("sfu.") + n, {t}, {t});

  // data movement: one op per direction (each is a distinct instruction).
  reg("tma.gmem_to_smem", {t}, {t});
  reg("tma.smem_to_gmem", {t}, {t});
  reg("lsu.smem_to_reg", {t}, {t});
  reg("lsu.reg_to_smem", {t}, {t});
  reg("lsu.gmem_to_reg", {t}, {t});
  reg("lsu.reg_to_gmem", {t}, {t});

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
  r.RegisterValueImpl<TensorImpl>("tile.tensor");

  // operation impls
  r.RegisterOperationImpl<MmaOp>("tile.tc.mma");
  r.RegisterOperationImpl<MmaAccOp>("tile.tc.mma_acc");
  r.RegisterOperationImpl<ReduceOp>("tile.warp.reduce.sum");
  r.RegisterOperationImpl<ReduceOp>("tile.warp.reduce.max");
  r.RegisterOperationImpl<BroadcastOp>("tile.warp.broadcast");

  // collectives reuse the kernel dialect's dist OperationImpls, keyed by the
  // tile.comm.* op names.
  r.RegisterOperationImpl<kernel::SendOp>("tile.comm.send");
  r.RegisterOperationImpl<kernel::RecvOp>("tile.comm.recv");
  r.RegisterOperationImpl<kernel::BroadcastOp>("tile.comm.broadcast");
  r.RegisterOperationImpl<kernel::AllToAllOp>("tile.comm.all_to_all");
  r.RegisterOperationImpl<kernel::AllReduceSum>("tile.comm.all_reduce.sum");
  r.RegisterOperationImpl<kernel::AllReduceMax>("tile.comm.all_reduce.max");
  r.RegisterOperationImpl<kernel::AllGather>("tile.comm.all_gather");
  r.RegisterOperationImpl<kernel::MoeDispatch>("tile.comm.moe_dispatch");
  r.RegisterOperationImpl<kernel::MoeCombine>("tile.comm.moe_combine");
}

}  // namespace

void RegisterTileDialect(serial::Registry& r) {
  RegisterOperators(r);
  RegisterImplCodecs(r);
}

}  // namespace kvm::ir::tile
