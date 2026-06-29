#include "dialects.h"

#include "dialect/kernel/dialect.h"
#include "dialect/task/dialect.h"
#include "dialect/tile/dialect.h"
#include "serialization/builtin_codecs.h"

namespace kvm::cli {

const std::vector<DialectEntry>& Dialects() {
  static const std::vector<DialectEntry> kDialects = {
      {"kernel", &kvm::ir::kernel::RegisterKernelDialect},
      {"task", &kvm::ir::task::RegisterTaskDialect},
      {"tile", &kvm::ir::tile::RegisterTileDialect},
  };
  return kDialects;
}

void RegisterAllDialects(kvm::ir::serial::Registry& r) {
  // builtin impl codecs (const values, etc.) are always available
  kvm::ir::serial::RegisterBuiltins(r);
  for (const auto& d : Dialects()) d.register_into(r);
}

}  // namespace kvm::cli
