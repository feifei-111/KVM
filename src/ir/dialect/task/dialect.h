#pragma once

// The task-graph dialect ("task"): registers its value/operation impl codecs
// and its operator signatures into a serialization Registry, so graphs using
// task.* ops can be serialized and parsed.
//
// The task dialect is what kernel-level composite ops get decomposed into: each
// op maps to one class of hardware unit on the SM (TC / ALU / SFU / LSU / warp
// reduction / NIC). See Notion "task graph" for the H200-based op signatures.
// Op names are task.<...>.

#include "serialization/registry.h"

namespace kvm::ir::task {

// Register every task-dialect operator + impl codec into `r`.
void RegisterTaskDialect(serial::Registry& r);

}  // namespace kvm::ir::task
