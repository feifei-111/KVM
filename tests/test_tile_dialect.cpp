// Tests for the tile-graph dialect: build with its ops/impls, serialize, parse
// back through the registered operators, and check round-trip. Mirrors the task
// dialect tests, plus the tile tensor's `slice` field.
#include <string>

#include "dialect/tile/dialect.h"
#include "dialect/tile/ops.h"
#include "dialect/tile/values.h"
#include "dialect/tile/verify.h"
#include "graph.h"
#include "serialization/parser.h"
#include "serialization/serializer.h"
#include "test_harness.h"

using namespace kvm::ir;
using namespace kvm::ir::tile;

namespace {
Type T() { return {"tensor", "tile"}; }
TensorImpl F16(std::vector<int> shape, Location loc = Location::kGlobal,
               std::vector<int> slice = {}) {
  return TensorImpl{std::move(shape), {}, Dtype::kF16, loc, std::move(slice)};
}
serial::Registry MakeRegistry() {
  serial::Registry r;
  RegisterTileDialect(r);
  return r;
}
}  // namespace

KVM_TEST(tensor_impl_round_trip_with_slice) {
  TensorImpl t = F16({8, 16}, Location::kRegister, {0, 8});
  TensorImpl back = TensorImpl::Deserialize(t.Serialize());
  KVM_CHECK(back.dtype == Dtype::kF16);
  KVM_CHECK(back.loc == Location::kRegister);
  KVM_CHECK(back.shape.size() == 2 && back.shape[0] == 8 &&
            back.shape[1] == 16);
  KVM_CHECK(back.slice.size() == 2 && back.slice[0] == 0 && back.slice[1] == 8);
}

KVM_TEST(tensor_impl_empty_slice_omitted) {
  TensorImpl t = F16({8, 16}, Location::kShared);  // no slice
  KVM_CHECK(t.Serialize().find("slice") == std::string::npos);
  KVM_CHECK(TensorImpl::Deserialize(t.Serialize()).slice.empty());
}

KVM_TEST(copy_ops_registered_by_direction) {
  auto reg = MakeRegistry();
  for (const char* name : {"tile.tma.gmem_to_smem", "tile.tma.smem_to_gmem",
                           "tile.lsu.smem_to_reg", "tile.lsu.reg_to_smem",
                           "tile.lsu.gmem_to_reg", "tile.lsu.reg_to_gmem"}) {
    KVM_CHECK(reg.GetOperator(name).has_value());
  }
}

KVM_TEST(graph_round_trip_with_tile_ops) {
  auto reg = MakeRegistry();

  Graph g;
  const Value* x =
      g.MakeInput("x", T(), F16({8, 16}, Location::kShared, {0, 8}));
  const Value* w = g.MakeInput("w", T(), F16({16, 16}, Location::kShared));
  Block* root = g.MakeBlock({x, w}, "entry");

  Operator mma = *reg.GetOperator("tile.tc.mma");
  const Operation* mm =
      g.MakeOperation(root, mma, MmaOp{8, 16, 16}, {x, w},
                      {Value{"y", T(), F16({8, 16}, Location::kRegister)}});
  const Value* y = g.GetOutputs(mm)[0];

  Operator rsum = *reg.GetOperator("tile.warp.reduce.sum");
  g.MakeOperation(root, rsum, ReduceOp{1}, {y},
                  {Value{"s", T(), F16({8, 1}, Location::kRegister)}});
  g.SetMain(root);

  std::string text1 = serial::Serialize(g, reg);
  Graph g2;
  serial::Parse(text1, reg, g2);
  std::string text2 = serial::Serialize(g2, reg);
  KVM_CHECK(text1 == text2);

  const Block* m = g2.main();
  const Operation* last = m->operations.back();
  const ReduceOp* impl = last->impl.as<ReduceOp>();
  KVM_CHECK(impl != nullptr && impl->axis == 1);
}

KVM_TEST(verify_mma_rejects_shape_mismatch) {
  // Note: a fully-legal mma currently CHECK-fails (legal-tile-shape table not
  // implemented), so we only assert the failures that trip *before* that point.
  auto reg = MakeRegistry();
  Graph g;
  const Value* a = g.MakeInput("a", T(), F16({8, 16}, Location::kShared));
  Block* root = g.MakeBlock({a}, "e");
  Operator mma = *reg.GetOperator("tile.tc.mma");

  // inner dims disagree (A.k=16, B.k=8) -> fails before the legality TODO
  const Value* b2 = g.MakeInput("b2", T(), F16({8, 32}, Location::kShared));
  const Operation* bad =
      g.MakeOperation(root, mma, MmaOp{8, 32, 16}, {a, b2},
                      {Value{"d2", T(), F16({8, 32}, Location::kRegister)}});
  KVM_CHECK(!Verify(bad, g.GetInputs(bad), g.GetOutputs(bad)).ok);
}

KVM_TEST(verify_mma_rejects_bad_accumulator_loc) {
  auto reg = MakeRegistry();
  Graph g;
  const Value* a = g.MakeInput("a", T(), F16({8, 16}, Location::kShared));
  const Value* b = g.MakeInput("b", T(), F16({16, 32}, Location::kShared));
  Block* root = g.MakeBlock({a, b}, "e");
  Operator mma = *reg.GetOperator("tile.tc.mma");
  // D in shared (must be register) -> illegal
  const Operation* bad =
      g.MakeOperation(root, mma, MmaOp{8, 32, 16}, {a, b},
                      {Value{"d", T(), F16({8, 32}, Location::kShared)}});
  KVM_CHECK(!Verify(bad, g.GetInputs(bad), g.GetOutputs(bad)).ok);
}

KVM_TEST(verify_move_direction) {
  auto reg = MakeRegistry();
  Graph g;
  const Value* s = g.MakeInput("s", T(), F16({8, 16}, Location::kShared));
  Block* root = g.MakeBlock({s}, "e");
  Operator ld = *reg.GetOperator("tile.lsu.smem_to_reg");
  // shared -> register : legal
  const Operation* ok = g.MakeOperation(
      root, ld, {}, {s}, {Value{"r", T(), F16({8, 16}, Location::kRegister)}});
  KVM_CHECK(Verify(ok, g.GetInputs(ok), g.GetOutputs(ok)).ok);
  // same op but output in shared : wrong dst loc
  const Operation* bad = g.MakeOperation(
      root, ld, {}, {s}, {Value{"r2", T(), F16({8, 16}, Location::kShared)}});
  KVM_CHECK(!Verify(bad, g.GetInputs(bad), g.GetOutputs(bad)).ok);
}

KVM_TEST(verify_elementwise_shape_mismatch) {
  auto reg = MakeRegistry();
  Graph g;
  const Value* x = g.MakeInput("x", T(), F16({8, 16}, Location::kRegister));
  const Value* y = g.MakeInput("y", T(), F16({8, 8}, Location::kRegister));
  Block* root = g.MakeBlock({x, y}, "e");
  Operator add = *reg.GetOperator("tile.alu.add");
  const Operation* bad =
      g.MakeOperation(root, add, {}, {x, y},
                      {Value{"z", T(), F16({8, 16}, Location::kRegister)}});
  KVM_CHECK(!Verify(bad, g.GetInputs(bad), g.GetOutputs(bad)).ok);
}

KVM_RUN_ALL()
