#pragma once

// The set of dialects the `kvm` tool knows about. Each entry can register its
// operators + impl codecs into a serialization Registry. This is the one place
// that lists available dialects, so `kvm show dialect` and IR loading agree.

#include <string>
#include <vector>

#include "serialization/registry.h"

namespace kvm::cli {

// A known dialect: its name (the `a.` prefix in op names) and a registrar.
struct DialectEntry {
  std::string name;
  void (*register_into)(kvm::ir::serial::Registry&);
};

// All dialects the tool ships with.
const std::vector<DialectEntry>& Dialects();

// Register every known dialect into `r` (used when loading arbitrary IR).
void RegisterAllDialects(kvm::ir::serial::Registry& r);

}  // namespace kvm::cli
