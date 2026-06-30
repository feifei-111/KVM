#pragma once

// kernel-graph -> task-graph transform.
//
// The transform DECOMPOSES each kernel operation into a task SUBGRAPH and
// splices those task ops flat into the result graph -- "inlining" the
// expansion, no nesting. (Internally we call this per-op expansion "decomp";
// the transform itself is named by what it does end to end: kernel -> task.)
// The driver here is deliberately dumb: it walks the kernel graph in program
// order and, for each op, asks the Device how to expand it (Device owns all
// per-op, per-hardware knowledge). The driver only threads values through:
//
//   - a kernel Value* -> task Value* map resolves each op's operands to their
//     already-produced task values; block inputs seed the map (a kernel tensor
//     becomes a task tensor in global memory).
//   - the task values a Device hook returns for an op's outputs are recorded so
//     later ops (and the block's returns) can refer to them.
//
// The kernel graph is read-only; the task graph is built in place (it owns its
// arena), mirroring serial::Parse's shape. A later simplification step over the
// produced task graph is a separate transform, not done here.
//
// The Device that drives the per-op expansion is built internally from the
// kernel graph's own config (Device is fully determined by GraphConfig), so the
// caller does not supply one -- this also rules out a device whose config
// disagrees with the graph.

#include "graph.h"

namespace kvm::transform {

// Transform `kernel` into `task`. `task` must be empty; it is built in place
// and its config is copied from `kernel`.
void TransformKernelToTask(const ir::Graph& kernel, ir::Graph& task);

}  // namespace kvm::transform
