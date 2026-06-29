#pragma once

// The kernel-graph dialect ("kernel"): registers its value/operation impl
// codecs and its operator signatures into a serialization Registry, so graphs
// using kernel.* ops can be serialized and parsed.
//
// Op signatures follow Notion "high level ir". Names are kernel.<...>; the rank
// info for dist/collective ops lives on their OperationImpl (dist.h), not as
// value inputs.

#include "serialization/registry.h"

namespace kvm::ir::kernel {

// Register every kernel-dialect operator + impl codec into `r`.
void RegisterKernelDialect(serial::Registry& r);

}  // namespace kvm::ir::kernel
