#pragma once

// Textual parser: the inverse of serializer.h. Reads the MLIR-ish text back
// into a Graph (plus an AttrMap for any `{...}` attrs).
//
// What is reconstructed and how:
//   - Type "dialect.name"      -> built directly from the two strings.
//   - Operator "dialect.name"  -> looked up in the Registry (a dialect
//                                 registers the full signature; the text only
//                                 carries the name).
//   - value/op impl <...>      -> the registered codec (T::Deserialize).
//   - %ref operands            -> resolved through a name -> Value* table built
//                                 as definitions are seen.
//   - nested block-op <...>    -> parsed recursively into a child Block.
//   - {attr}                   -> JSON-like scalars into the AttrMap.
//
// Everything lives under serialization/; core IR headers are untouched.
// Malformed input is a hard error: parsing CHECK-fails (aborts) rather than
// returning a status -- this is an early, self-used tool.

#include <string>

#include "attr.h"
#include "graph.h"
#include "serialization/registry.h"

namespace kvm::ir::serial {

// Parse text into the given (empty) graph, returning the attrs found. The graph
// is built in place (it owns the arena); `registry` supplies operator
// definitions and impl codecs.
AttrMap Parse(const std::string& text, const Registry& registry, Graph& graph);

}  // namespace kvm::ir::serial
