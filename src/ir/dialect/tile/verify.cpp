#include "dialect/tile/verify.h"

#include <glog/logging.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "dialect/tile/ops.h"
#include "dialect/tile/values.h"
#include "node.h"

namespace kvm::ir::tile {
namespace {

// Pull the tile::TensorImpl off a value, or nullptr if it isn't one.
const TensorImpl* AsTensor(const Value* v) {
  return v ? v->impl.as<TensorImpl>() : nullptr;
}

// Every tile operand must be a tile.tensor with an impl. Returns a reason if
// not, else empty.
std::string AllTensors(std::span<const Value* const> vs, const char* role) {
  for (const Value* v : vs) {
    if (!AsTensor(v))
      return std::string(role) + " operand is not a tile.tensor value";
  }
  return {};
}

VerifyResult ExpectArity(std::string_view name,
                         std::span<const Value* const> ins, std::size_t nin,
                         std::span<const Value* const> outs, std::size_t nout) {
  if (ins.size() != nin)
    return VerifyResult::Fail(std::string(name) + ": expected " +
                              std::to_string(nin) + " inputs, got " +
                              std::to_string(ins.size()));
  if (outs.size() != nout)
    return VerifyResult::Fail(std::string(name) + ": expected " +
                              std::to_string(nout) + " outputs, got " +
                              std::to_string(outs.size()));
  return VerifyResult::Ok();
}

bool SameShape(const TensorImpl* a, const TensorImpl* b) {
  return a->shape == b->shape;
}

// All operands are tensors with identical shape; n inputs, 1 output.
VerifyResult Elementwise(std::string_view name,
                         std::span<const Value* const> ins,
                         std::span<const Value* const> outs, std::size_t nin) {
  if (auto a = ExpectArity(name, ins, nin, outs, 1); !a.ok) return a;
  if (auto e = AllTensors(ins, "input"); !e.empty())
    return VerifyResult::Fail(std::string(name) + ": " + e);
  if (auto e = AllTensors(outs, "output"); !e.empty())
    return VerifyResult::Fail(std::string(name) + ": " + e);
  const TensorImpl* o = AsTensor(outs[0]);
  for (const Value* v : ins)
    if (!SameShape(AsTensor(v), o))
      return VerifyResult::Fail(std::string(name) +
                                ": input/output shape mismatch");
  return VerifyResult::Ok();
}

// Data movement: 1 input at `from`, 1 output at `to`, same shape.
VerifyResult Move(std::string_view name, std::span<const Value* const> ins,
                  std::span<const Value* const> outs, Location from,
                  Location to) {
  if (auto a = ExpectArity(name, ins, 1, outs, 1); !a.ok) return a;
  const TensorImpl* i = AsTensor(ins[0]);
  const TensorImpl* o = AsTensor(outs[0]);
  if (!i || !o)
    return VerifyResult::Fail(std::string(name) +
                              ": operand not a tile.tensor");
  if (i->loc != from || o->loc != to)
    return VerifyResult::Fail(std::string(name) + ": wrong src/dst location");
  if (!SameShape(i, o))
    return VerifyResult::Fail(std::string(name) + ": src/dst shape mismatch");
  return VerifyResult::Ok();
}

// matmul shape relation: A[m,k] · B[k,n] -> D[m,n]. acc adds C[m,n]. The op
// impl's (im,in,ik) is the hardware tile shape this instance realizes; the
// operand shapes must match it. Whether (im,in,ik) is itself a hardware-legal
// wgmma tile is a fixed rule of the op -- left as TODO until the legal-shape
// table is written.
VerifyResult Mma(std::string_view name, std::span<const Value* const> ins,
                 std::span<const Value* const> outs, bool acc, int im, int in,
                 int ik) {
  std::size_t nin = acc ? 3 : 2;
  if (auto a = ExpectArity(name, ins, nin, outs, 1); !a.ok) return a;
  if (auto e = AllTensors(ins, "input"); !e.empty())
    return VerifyResult::Fail(std::string(name) + ": " + e);
  if (auto e = AllTensors(outs, "output"); !e.empty())
    return VerifyResult::Fail(std::string(name) + ": " + e);
  const TensorImpl* a = AsTensor(ins[0]);
  const TensorImpl* b = AsTensor(ins[1]);
  const TensorImpl* d = AsTensor(outs[0]);
  if (a->shape.size() != 2 || b->shape.size() != 2 || d->shape.size() != 2)
    return VerifyResult::Fail(std::string(name) + ": operands must be 2-D");
  int m = a->shape[0], k = a->shape[1];
  if (b->shape[0] != k)
    return VerifyResult::Fail(std::string(name) + ": A.k != B.k");
  int n = b->shape[1];
  if (d->shape[0] != m || d->shape[1] != n)
    return VerifyResult::Fail(std::string(name) + ": D shape != [m,n]");
  // operand shapes must match the tile shape the op claims to realize.
  if (m != im || n != in || k != ik)
    return VerifyResult::Fail(std::string(name) +
                              ": operand shape != op (m,n,k)");
  // wgmma: B in shared, accumulator D in register.
  if (b->loc != Location::kShared)
    return VerifyResult::Fail(std::string(name) + ": B must be in shared");
  if (d->loc != Location::kRegister)
    return VerifyResult::Fail(std::string(name) + ": D must be in register");
  if (acc) {
    const TensorImpl* c = AsTensor(ins[2]);
    if (!SameShape(c, d) || c->loc != Location::kRegister)
      return VerifyResult::Fail(std::string(name) +
                                ": C must match D in register");
  }
  // The remaining rule -- whether (m,n,k) is a hardware-legal wgmma tile -- is
  // not implemented. Not done != legal, so fail hard rather than pass silently.
  CHECK(false) << name << ": legal-tile-shape check not implemented";
  return VerifyResult::Ok();  // unreachable
}

}  // namespace

VerifyResult Verify(const OpNode* op) {
  // Pull the payload values out of the node's operand/result edges.
  std::vector<const Value*> in_vals;
  std::vector<const Value*> out_vals;
  in_vals.reserve(op->operands().size());
  out_vals.reserve(op->results().size());
  for (const ValueNode* v : op->operands()) in_vals.push_back(&v->value());
  for (const ValueNode* v : op->results()) out_vals.push_back(&v->value());
  std::span<const Value* const> ins(in_vals);
  std::span<const Value* const> outs(out_vals);

  const std::string& n = op->op().op.name;

  if (n == "tc.mma") {
    const MmaOp* mm = op->op().impl.as<MmaOp>();
    if (!mm) return VerifyResult::Fail("tc.mma: missing MmaOp impl");
    return Mma(n, ins, outs, /*acc=*/false, mm->m, mm->n, mm->k);
  }
  if (n == "tc.mma_acc") {
    const MmaAccOp* mm = op->op().impl.as<MmaAccOp>();
    if (!mm) return VerifyResult::Fail("tc.mma_acc: missing MmaAccOp impl");
    return Mma(n, ins, outs, /*acc=*/true, mm->m, mm->n, mm->k);
  }

  // ALU unary / binary / ternary elementwise (same-shape).
  if (n == "alu.neg" || n == "alu.abs") return Elementwise(n, ins, outs, 1);
  if (n == "alu.add" || n == "alu.sub" || n == "alu.mul" || n == "alu.max" ||
      n == "alu.min" || n == "alu.cmp.eq" || n == "alu.cmp.ne" ||
      n == "alu.cmp.lt" || n == "alu.cmp.le" || n == "alu.cmp.gt" ||
      n == "alu.cmp.ge")
    return Elementwise(n, ins, outs, 2);
  if (n == "alu.select") return Elementwise(n, ins, outs, 3);
  if (n == "alu.cast.f16" || n == "alu.cast.f32" || n == "alu.cast.bf16" ||
      n == "alu.cast.i32")
    return Elementwise(n, ins, outs, 1);

  // SFU unary (same-shape).
  if (n == "sfu.rcp" || n == "sfu.rsqrt" || n == "sfu.sqrt" || n == "sfu.exp" ||
      n == "sfu.log" || n == "sfu.sin" || n == "sfu.cos")
    return Elementwise(n, ins, outs, 1);

  // data movement, by direction.
  if (n == "tma.gmem_to_smem")
    return Move(n, ins, outs, Location::kGlobal, Location::kShared);
  if (n == "tma.smem_to_gmem")
    return Move(n, ins, outs, Location::kShared, Location::kGlobal);
  if (n == "lsu.smem_to_reg")
    return Move(n, ins, outs, Location::kShared, Location::kRegister);
  if (n == "lsu.reg_to_smem")
    return Move(n, ins, outs, Location::kRegister, Location::kShared);
  if (n == "lsu.gmem_to_reg")
    return Move(n, ins, outs, Location::kGlobal, Location::kRegister);
  if (n == "lsu.reg_to_gmem")
    return Move(n, ins, outs, Location::kRegister, Location::kGlobal);

  // Not implemented yet: warp reduce/broadcast (axis-driven shape) and comm.*.
  // Not done != legal -- fail hard rather than pass silently.
  if (n.rfind("warp.", 0) == 0 || n.rfind("comm.", 0) == 0)
    CHECK(false) << "tile::Verify: verifier for op '" << n
                 << "' not implemented";

  CHECK(false) << "tile::Verify: no verifier for op '" << n << "'";
  return VerifyResult::Ok();  // unreachable
}

}  // namespace kvm::ir::tile
