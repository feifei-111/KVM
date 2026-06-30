#pragma once

// Verify hook for the tile-graph dialect.
//
// Verify(op, ins, outs) dispatches on op->op.name to the per-op rule and
// returns whether this operation's inputs/outputs are legal. It is a plain
// switch over the tile op set (known at compile time), not a registry. Each
// dialect owns its own Verify; this one only knows tile.* ops.
//
// What is checked now: operand count, dtype agreement, storage location, and
// shape relations -- the fixed rules of each op. What is deferred: anything
// that would need a device (e.g. "is this mnk a legal wgmma tile"); by the
// naming rule such a difference is a different op, not a device query here.
// Deferred ops fall through to Ok().

#include "node.h"
#include "verify_result.h"

namespace kvm::ir::tile {

// Verify one tile operation. The op node carries its own input/output values
// (operands/results), so that is all that is needed. Returns ok, or a reason
// on failure.
VerifyResult Verify(const OpNode* op);

}  // namespace kvm::ir::tile
