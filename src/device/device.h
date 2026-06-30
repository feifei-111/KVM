#pragma once

// Device: the decomp backend.
//
// What decomp needs is two things, and Device is exactly those two things:
//   1. a per-kernel-op LOWERING HOOK -- how a kernel op expands into a task
//      subgraph; and
//   2. DEVICE INFO -- the hardware facts a hook consults (smem size, legal tile
//      shapes, rank topology, ...).
// How a kernel op decomposes is owned entirely by the device, not by the
// transform: the transform is a dumb driver that asks the device per op. The
// device may branch on hardware (h200 vs b200, ...) inside each hook; the
// driver never sees that.
//
// Device is a TOOL, not a core IR entity: it is built from a GraphConfig and
// carries no graph state, so it is created where decomp runs and passed by
// const ref. (It could equally be a singleton keyed by config; a plain object
// is kept here because it is trivially testable.)
//
// Hook dispatch is a switch on the kernel op name (the op set is known at
// compile time), mirroring the verify hooks. An op with no lowering yet
// CHECK-fails rather than silently dropping work.

#include <span>
#include <string>
#include <vector>

#include "graph.h"
#include "graph_config.h"
#include "serialization/registry.h"
#include "value.h"

namespace kvm::device {

class Device {
 public:
  explicit Device(const ir::GraphConfig& config);

  // Expand one kernel operation into task ops emitted into `block`.
  //
  //   kernel_op      -- the op node being lowered (its name selects the hook;
  //                     its impl carries any constants, e.g. collective ranks).
  //   inputs         -- the TASK value nodes the op's operands map to (already
  //                     translated by the driver), in operand order.
  //   input_names    -- the op's KERNEL operand names, used to build readable,
  //                     lineage-carrying names for the subgraph's inner values
  //                     (base = join(input_names,'_') + '_' + opname).
  //   kernel_outputs -- the op's KERNEL output value nodes, read for result
  //                     shape/dtype (task tiling happens later, so a result
  //                     keeps the kernel shape, gaining only a location).
  //
  // Returns the task value nodes corresponding to the kernel op's outputs.
  std::vector<ir::ValueNode*> Decomp(
      const ir::OpNode* kernel_op, std::span<ir::ValueNode* const> inputs,
      std::span<const std::string> input_names,
      std::span<const ir::ValueNode* const> kernel_outputs,
      ir::Block* block) const;

 private:
  ir::GraphConfig config_;    // device info source (kv meta, ranks, ...)
  ir::serial::Registry ops_;  // task operator definitions (for signatures)
};

}  // namespace kvm::device
