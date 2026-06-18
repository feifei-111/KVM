#include "dialect/kernel/dialect.h"

#include <string>
#include <vector>

#include "dialect/kernel/attn.h"
#include "dialect/kernel/dist.h"
#include "dialect/kernel/tensor.h"
#include "value.h"

namespace kvm::ir::kernel {
namespace {

constexpr const char* kDialect = "kernel";

Type Tensor() { return {"tensor", kDialect}; }
Type AttnMeta() { return {"attn_meta", kDialect}; }

// Build an operator "kernel.<name>" with the given input/output types.
Operator Op(std::string name, std::vector<Type> ins, std::vector<Type> outs) {
  return Operator{std::move(name), kDialect, std::move(ins), std::move(outs)};
}

void RegisterOperators(serial::Registry& r) {
  const Type t = Tensor();
  const Type meta = AttnMeta();

  auto reg = [&](const std::string& name, std::vector<Type> ins,
                 std::vector<Type> outs) {
    r.RegisterOperator(std::string(kDialect) + "." + name,
                       Op(name, std::move(ins), std::move(outs)));
  };

  // matmul / gemm
  reg("matmul", {t, t}, {t});
  reg("gemm", {t, t, t}, {t});

  // attention (dynamic batch info travels in the attn_meta value)
  reg("attn.prefill", {t, t, t, meta}, {t});
  reg("attn.decode", {t, t, t, meta}, {t});
  reg("attn.rope", {t, meta}, {t});

  // arith binary / unary
  for (const char* n : {"add", "sub", "mul", "div", "max", "min"})
    reg(n, {t, t}, {t});
  for (const char* n : {"neg", "exp", "log", "sqrt", "rsqrt", "abs"})
    reg(n, {t}, {t});

  // compare (output dtype=bool carried by the result value's TensorImpl)
  for (const char* n :
       {"cmp.eq", "cmp.ne", "cmp.lt", "cmp.le", "cmp.gt", "cmp.ge"})
    reg(n, {t, t}, {t});

  // cast (target dtype is in the op name suffix + the result's TensorImpl)
  for (const char* n : {"cast.f16", "cast.f32", "cast.bf16", "cast.i32"})
    reg(n, {t}, {t});

  // comm (rank info on the OperationImpl)
  reg("comm.send", {t}, {t});
  reg("comm.recv", {t}, {t});
  reg("comm.broadcast", {t}, {t});
  reg("comm.all_to_all", {t}, {t});
  reg("comm.all_reduce.sum", {t}, {t});
  reg("comm.all_reduce.max", {t}, {t});
  reg("comm.all_gather", {t}, {t});
  reg("comm.moe_dispatch", {t, t}, {t});
  reg("comm.moe_combine", {t, t}, {t});

  // kvcache
  reg("kvcache.read", {t, meta}, {t, t});
  reg("kvcache.write", {t, meta, t, t}, {t});
}

void RegisterImplCodecs(serial::Registry& r) {
  // value impls
  r.RegisterValueImpl<TensorImpl>("kernel.tensor");
  r.RegisterValueImpl<AttnMetaImpl>("kernel.attn_meta");

  // operation impls (dist / collectives), keyed by their op name
  r.RegisterOperationImpl<SendOp>("kernel.comm.send");
  r.RegisterOperationImpl<RecvOp>("kernel.comm.recv");
  r.RegisterOperationImpl<BroadcastOp>("kernel.comm.broadcast");
  r.RegisterOperationImpl<AllToAllOp>("kernel.comm.all_to_all");
  r.RegisterOperationImpl<AllReduceSum>("kernel.comm.all_reduce.sum");
  r.RegisterOperationImpl<AllReduceMax>("kernel.comm.all_reduce.max");
  r.RegisterOperationImpl<AllGather>("kernel.comm.all_gather");
  r.RegisterOperationImpl<MoeDispatch>("kernel.comm.moe_dispatch");
  r.RegisterOperationImpl<MoeCombine>("kernel.comm.moe_combine");
}

}  // namespace

void RegisterKernelDialect(serial::Registry& r) {
  RegisterOperators(r);
  RegisterImplCodecs(r);
}

}  // namespace kvm::ir::kernel
