#pragma once

// Runtime attributes for the KVM IR.
//
// ADT (runtime):
//   Attr    :: String -> Any
//   AttrMap :: (Value | Operation | Block) -> Attr
//   SetAttr :: AttrMap -> (Value | Operation | Block) -> String -> Any ->
//   AttrMap
//
// Attributes are dynamic data that passes attach to nodes (analysis results,
// schedule hints, ...). They are kept OUTSIDE the immutable nodes: a node never
// carries its attrs, so immutability is preserved and attrs can come and go per
// pass. The host is `Value | Operation | Block`, but attrs are unrelated to the
// host's content -- the host is only a key (a pointer), so attr storage does
// not touch the node.
//
// `Any` here is a true top type (std::any), not a mark/open-sum: passes attach
// arbitrary analysis data with no fixed member set. (Contrast ValueImpl, which
// is a bounded open sum.)
//
// SetAttr is functional at the ADT level (it conceptually yields a new map with
// no hidden side effect). The implementation uses a mutable map -- the only way
// to express this without a persistent-map library or per-set full copies. This
// is the usual ADT-semantics / implementation-means split (same as edges).

#include <any>
#include <string>
#include <unordered_map>
#include <variant>

#include "node.h"

namespace kvm::ir {

// Host :: ValueNode | OpNode | Block  (referred to by non-owning pointer). The
// host is the topology NODE, the stable identity passes refer to; attrs are
// unrelated to the node's payload content -- the node is only a key.
using AttrHost = std::variant<const ValueNode*, const OpNode*, const Block*>;

// Attr :: String -> Any  (one host's attribute bag).
using Attr = std::unordered_map<std::string, std::any>;

// AttrMap :: (Value | Operation | Block) -> Attr.
class AttrMap {
 public:
  // SetAttr :: AttrMap -> Host -> String -> Any -> AttrMap.
  // Functional in the ADT; mutates in place in the implementation. Returns
  // *this so chained set is convenient.
  AttrMap& Set(AttrHost host, std::string key, std::any value) {
    map_[host][std::move(key)] = std::move(value);
    return *this;
  }

  // Look up one attribute; nullptr if the host has no such key.
  const std::any* Get(AttrHost host, const std::string& key) const {
    auto h = map_.find(host);
    if (h == map_.end()) return nullptr;
    auto a = h->second.find(key);
    return a == h->second.end() ? nullptr : &a->second;
  }

  bool Has(AttrHost host, const std::string& key) const {
    return Get(host, key) != nullptr;
  }

  // Remove one attribute; returns true if it was present.
  bool Remove(AttrHost host, const std::string& key) {
    auto h = map_.find(host);
    if (h == map_.end()) return false;
    return h->second.erase(key) > 0;
  }

  // The whole attribute bag for a host (empty if none).
  const Attr* GetAttr(AttrHost host) const {
    auto h = map_.find(host);
    return h == map_.end() ? nullptr : &h->second;
  }

 private:
  std::unordered_map<AttrHost, Attr> map_;
};

}  // namespace kvm::ir
