# Render Design

> **TL;DR:** Renderers are views over KVM IR. They live outside core IR, reuse
> one Graphviz DOT layout, and keep values as first-class graph nodes.

## Context

KVM IR is a value/operation graph. Values are nodes, operations are nodes, and
edges describe producer/operand relationships. This is intentionally different
from operator-only formats where tensors are modeled as edge labels.

The render module exists so visualization choices do not leak into
`compiler::ir`. The dependency direction is:

```text
src/render -> compiler::ir
compiler::ir -> no render dependency
```

The CLI should load and verify IR, then call this module for visual outputs.

## Outputs

The stable render surface is:

```text
render_dot(graph, options) -> DOT text
write_dot(graph, path, options)
write_png(graph, path, options)
write_html(graph, path, options)
```

PNG and HTML both derive from the same DOT:

- `write_png` runs `dot -Tpng`.
- `write_html` runs `dot -Tsvg` and embeds the SVG in a standalone HTML page.

This keeps the static image and interactive view visually consistent.

## HTML Inspector

The HTML viewer should not draw a second graph layout. It embeds the Graphviz
SVG and adds interaction around it.

Graphviz emits each node as a SVG group with a `<title>` containing the DOT node
name, such as `value0` or `op3`. The HTML code uses that title as the lookup key
into a side table generated from the IR graph. Clicking a value or operation
selects the SVG node and shows type, attr, input, and output details in the
inspector.

Inspector attrs are always shown, including empty `{}` maps. Non-empty attrs are
pretty-printed as indented JSON so graph properties such as `dist.config` remain
readable without bloating the node labels.

Keep this id convention stable:

```text
value{index}
op{index}
graph
```

Edges are not selectable today. If they become selectable later, do not collapse
values into edge labels; KVM IR values are real graph nodes.

## Interaction

The current HTML interaction model is:

- click value/operation node: select and inspect
- drag empty canvas: pan
- trackpad two-finger scroll: pan
- pinch gesture or Ctrl-wheel: zoom
- Fit button: reset viewBox
- right divider: resize inspector

Node clicks must not start canvas drag. This is why canvas `pointerdown` exits
early when the target is inside `g.node`.

## Non-Goals

Render is not a semantic export layer. In particular:

- It should not define dialect behavior.
- It should not replace IR text serialization.
- It should not lower KVM IR into ONNX or other operator-only graph formats.
- It should not add storage-specific APIs to core IR just for visualization.

If a renderer needs more detail, prefer adding a normal graph query to
`compiler::ir` over reaching into storage layout.
