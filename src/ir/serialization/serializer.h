#pragma once

// Textual serializer for a Graph (MLIR-ish).
//
// Shape:
//   graph {
//     config { ranks = [0, 1, 2, 3] }
//     ^block(%a: hlo.tensor<...> {attr}, ...) {
//       %c: hlo.tensor = hlo.matmul(%a, %b)
//       %d: builtin.int<42> {rank = 3} = builtin.const()
//       hlo.print(%c)
//     }
//   }
//
// Rules:
//   - value def:  %name: dialect.type<impl> {attr}
//   - operation:  <outs> = dialect.op<impl> {attr}(<operand %refs>)
//                 (no name on operations; omit "<outs> =" when there are none)
//   - operand:    just %name
//   - empty <impl> or {attr} is omitted entirely (no `<>` / `{}`).
//   - BlockOpImpl renders its child block as the op's <impl> (recursive).
//
// Leaf impls serialize themselves via the Registry (which calls T::Serialize).
// Attrs are treated as JSON-like values (see attr_json.h).

#include <string>

#include "attr.h"
#include "graph.h"
#include "serialization/registry.h"

namespace kvm::ir::serial {

// Serialize a whole graph. `attrs` is optional (nullptr = no attrs printed).
std::string Serialize(const Graph& graph, const Registry& registry,
                      const AttrMap* attrs = nullptr);

}  // namespace kvm::ir::serial
