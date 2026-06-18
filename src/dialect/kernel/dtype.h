#pragma once

// Dtype for the kernel-graph dialect. A small enum with text round-tripping,
// used by TensorImpl. Kept dialect-local (the core IR has no dtype concept).

#include <string>
#include <string_view>

namespace kvm::ir::kernel {

enum class Dtype {
  kI16,
  kI32,
  kF16,
  kBf16,
  kF32,
  kBool,
};

inline std::string DtypeName(Dtype d) {
  switch (d) {
    case Dtype::kI16:
      return "i16";
    case Dtype::kI32:
      return "i32";
    case Dtype::kF16:
      return "f16";
    case Dtype::kBf16:
      return "bf16";
    case Dtype::kF32:
      return "f32";
    case Dtype::kBool:
      return "bool";
  }
  return "f32";
}

inline Dtype DtypeFromName(std::string_view s) {
  if (s == "i16") return Dtype::kI16;
  if (s == "i32") return Dtype::kI32;
  if (s == "f16") return Dtype::kF16;
  if (s == "bf16") return Dtype::kBf16;
  if (s == "bool") return Dtype::kBool;
  return Dtype::kF32;
}

}  // namespace kvm::ir::kernel
