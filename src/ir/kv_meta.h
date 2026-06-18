#pragma once

// KV cache layout metadata, carried on the graph as read-only global truth.
// This is plain data depended on by both the core (GraphConfig) and dialects;
// it lives in its own header so neither side pulls in the other.
//
//   KVLayout :: Enum("layer_first", "page_first")
//   KVMeta   :: (layout, layer_size, block_size, token_bytes)

#include <string>
#include <string_view>

namespace kvm::ir {

enum class KVLayout {
  kLayerFirst,
  kPageFirst,
};

inline std::string KVLayoutName(KVLayout l) {
  return l == KVLayout::kPageFirst ? "page_first" : "layer_first";
}

inline KVLayout KVLayoutFromName(std::string_view s) {
  return s == "page_first" ? KVLayout::kPageFirst : KVLayout::kLayerFirst;
}

// KVMeta :: (layout, layer_size, block_size, token_bytes)
struct KVMeta {
  KVLayout layout = KVLayout::kLayerFirst;
  int layer_size = 0;
  int block_size = 0;
  int token_bytes = 0;
};

}  // namespace kvm::ir
