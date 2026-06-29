#pragma once

// The graph's configuration: read-only global truth carried on the graph,
// independent of its node/edge structure. Kept in its own header (not in
// graph.h) so it can be depended on by both the core and the dialects without
// pulling in the whole graph, and so "what configures a graph" lives in one
// place.
//
//   KVLayout    :: Enum("layer_first", "page_first")
//   KVMeta      :: (layout, layer_size, block_size, token_bytes)
//   DistConfig  :: (all_ranks: [Int])
//   GraphConfig :: (DistConfig, KVMeta)

#include <string>
#include <string_view>
#include <vector>

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

// DistConfig :: (all_ranks: [Int]) -- minimal for now; to be expanded later.
struct DistConfig {
  std::vector<int> all_ranks;
};

// GraphConfig :: (DistConfig, KVMeta)
struct GraphConfig {
  DistConfig dist;
  KVMeta kv;
};

}  // namespace kvm::ir
