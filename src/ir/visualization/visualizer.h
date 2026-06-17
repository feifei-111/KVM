#pragma once

// HTML visualizer for a Graph.
//
// Ported from the KVM_rust render module. Values and operations are both graph
// NODES (values are not collapsed into edge labels): a value box feeds an
// operation ellipse, which feeds its result value boxes.
//
// Pipeline (same as the Rust version):
//   Graph -> DOT text -> `dot -Tsvg` -> SVG embedded in a standalone HTML page
//            + an inspector side panel driven by a per-node detail table.
//
// The HTML is the only artifact; DOT and SVG are intermediate. Layout uses
// Graphviz `dot` at generation time (RenderHtml shells out to it); the produced
// HTML itself is self-contained and needs no tools to view.
//
// Node id convention (stable, used as the inspector lookup key, matching the
// SVG <title> Graphviz emits):
//   value{index}   op{index}   graph

#include <string>

#include "attr.h"
#include "graph.h"

namespace kvm::ir::viz {

// Render the graph to Graphviz DOT text (no external tools needed).
std::string RenderDot(const Graph& graph);

// Render the graph to a standalone HTML page. Requires the `dot` binary
// (Graphviz) on PATH at call time; throws std::runtime_error if it is missing
// or fails. `attrs` is optional (nullptr = none shown in the inspector).
std::string RenderHtml(const Graph& graph, const AttrMap* attrs = nullptr);

}  // namespace kvm::ir::viz
