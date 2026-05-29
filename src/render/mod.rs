use std::collections::BTreeSet;
use std::fs;
use std::io::Write;
use std::path::Path;
use std::process::{Command, Stdio};

use crate::compiler::ir::{
    Graph, IrError, Operation, TypeExpr, Value, ValueKind, format_attr_map_pretty,
};

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct GraphRenderOptions {
    pub show_types: bool,
    pub show_attrs: bool,
}

pub fn render_dot(
    graph: &Graph,
    _options: GraphRenderOptions,
) -> Result<String, String> {
    let mut out = String::from("digraph ir {\n");
    out.push_str("  graph [rankdir=TB, ranksep=0.85, nodesep=0.35];\n");
    out.push_str("  node [fontname=\"Menlo\"];\n");
    out.push_str("  edge [fontname=\"Menlo\"];\n");

    let outputs = output_values(graph)?;
    let mut emitted_values = BTreeSet::new();

    if let Some(block) = graph.entry_block() {
        for value in graph.block_inputs(block).map_err(render_err)? {
            emit_value_node(graph, &mut out, &outputs, &mut emitted_values, *value)?;
        }
        for value in graph.block_outputs(block).map_err(render_err)? {
            emit_value_node(graph, &mut out, &outputs, &mut emitted_values, *value)?;
        }
    }

    for op in graph.topological_operations().map_err(render_err)? {
        let view = graph.operation(op).map_err(render_err)?;
        let op_label =
            operation_label(graph.type_expr(view.ty).map_err(render_err)?, op);
        out.push_str(&format!(
            "  op{} [label=\"{}\", shape=ellipse, style=\"filled\", fillcolor=\"#fde68a\", color=\"#b45309\"];\n",
            op.index(),
            op_label
        ));
        for operand in view.operands {
            emit_value_node(graph, &mut out, &outputs, &mut emitted_values, *operand)?;
            out.push_str(&format!("  value{} -> op{};\n", operand.index(), op.index()));
        }
        for result in view.results {
            emit_value_node(graph, &mut out, &outputs, &mut emitted_values, *result)?;
            out.push_str(&format!("  op{} -> value{};\n", op.index(), result.index()));
        }
    }
    out.push('}');
    Ok(out)
}

pub fn write_dot(
    graph: &Graph,
    path: &str,
    options: GraphRenderOptions,
) -> Result<(), String> {
    write_file(path, &render_dot(graph, options)?)
}

pub fn write_png(
    graph: &Graph,
    path: &str,
    options: GraphRenderOptions,
) -> Result<(), String> {
    let dot = render_dot(graph, options)?;
    render_png(&dot, path)
}

pub fn write_html(
    graph: &Graph,
    path: &str,
    options: GraphRenderOptions,
) -> Result<(), String> {
    let dot = render_dot(graph, options)?;
    let svg = render_svg(&dot)?;
    let details = html_details_js(&html_details(graph)?);
    write_file(path, &wrap_svg_html(&svg, &details))
}

fn emit_value_node(
    graph: &Graph,
    out: &mut String,
    outputs: &BTreeSet<Value>,
    emitted_values: &mut BTreeSet<Value>,
    value: Value,
) -> Result<(), String> {
    if !emitted_values.insert(value) {
        return Ok(());
    }
    let view = graph.value(value).map_err(render_err)?;
    let label = value_label(&value_name(graph, value)?);
    let style = value_style(view.kind, outputs, value);
    out.push_str(&format!(
        "  value{} [label=\"{}\", shape=box, style=\"filled\", fillcolor=\"{}\", color=\"{}\"];\n",
        value.index(),
        label,
        style.fill,
        style.stroke,
    ));
    Ok(())
}

fn output_values(graph: &Graph) -> Result<BTreeSet<Value>, String> {
    let Some(block) = graph.entry_block() else {
        return Ok(BTreeSet::new());
    };
    Ok(graph.block_outputs(block).map_err(render_err)?.iter().copied().collect())
}

fn operation_label(ty: &TypeExpr, op: Operation) -> String {
    let label = if ty.dialect().is_empty() || ty.kind().is_empty() {
        format!("operation{}", op.index())
    } else {
        format!("{}.{}", ty.dialect(), ty.kind())
    };
    dot_escape(&label)
}

fn value_label(name: &str) -> String {
    dot_escape(&format!("%{name}"))
}

fn value_name(graph: &Graph, value: Value) -> Result<String, String> {
    let view = graph.value(value).map_err(render_err)?;
    Ok(view.name.map_or_else(|| format!("v{}", value.index()), str::to_string))
}

fn html_details(graph: &Graph) -> Result<Vec<GraphHtmlDetail>, String> {
    let mut details = vec![GraphHtmlDetail {
        id: "graph".to_string(),
        title: "Graph".to_string(),
        detail: format!("attrs:\n{}", format_attr_map_pretty(graph.properties())),
    }];
    let mut emitted_values = BTreeSet::new();

    if let Some(block) = graph.entry_block() {
        for value in graph.block_inputs(block).map_err(render_err)? {
            push_value_detail(graph, &mut emitted_values, *value, &mut details)?;
        }
        for value in graph.block_outputs(block).map_err(render_err)? {
            push_value_detail(graph, &mut emitted_values, *value, &mut details)?;
        }
    }

    for op in graph.topological_operations().map_err(render_err)? {
        let view = graph.operation(op).map_err(render_err)?;
        let ty = graph.type_expr(view.ty).map_err(render_err)?;
        let operands = value_list(graph, view.operands)?;
        let results = value_list(graph, view.results)?;
        let detail = vec![
            format!("operation: {}.{}", ty.dialect(), ty.kind()),
            format!("inputs: {operands}"),
            format!("outputs: {results}"),
            format!("attrs:\n{}", format_attr_map_pretty(view.attrs)),
        ];
        details.push(GraphHtmlDetail {
            id: format!("op{}", op.index()),
            title: format!("{}.{}", ty.dialect(), ty.kind()),
            detail: detail.join("\n"),
        });

        for value in view.operands {
            push_value_detail(graph, &mut emitted_values, *value, &mut details)?;
        }
        for value in view.results {
            push_value_detail(graph, &mut emitted_values, *value, &mut details)?;
        }
    }

    Ok(details)
}

fn push_value_detail(
    graph: &Graph,
    emitted_values: &mut BTreeSet<Value>,
    value: Value,
    details: &mut Vec<GraphHtmlDetail>,
) -> Result<(), String> {
    if !emitted_values.insert(value) {
        return Ok(());
    }
    let view = graph.value(value).map_err(render_err)?;
    let name = value_name(graph, value)?;
    let ty = graph.type_expr(view.ty).map_err(render_err)?;
    let mut detail = vec![format!("name: %{name}"), format!("type: {ty}")];
    detail.push(format!("attrs:\n{}", format_attr_map_pretty(view.attrs)));
    details.push(GraphHtmlDetail {
        id: format!("value{}", value.index()),
        title: format!("%{name}"),
        detail: detail.join("\n"),
    });
    Ok(())
}

fn value_list(graph: &Graph, values: &[Value]) -> Result<String, String> {
    if values.is_empty() {
        return Ok("()".to_string());
    }
    Ok(values
        .iter()
        .map(|value| value_name(graph, *value).map(|name| format!("%{name}")))
        .collect::<Result<Vec<_>, _>>()?
        .join(", "))
}

fn write_file(path: &str, content: &str) -> Result<(), String> {
    write_file_bytes(path, content.as_bytes())
}

fn write_file_bytes(path: &str, content: &[u8]) -> Result<(), String> {
    if let Some(parent) = Path::new(path).parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent).map_err(|err| {
                format!("failed to create {}: {err}", parent.display())
            })?;
        }
    }
    fs::write(path, content).map_err(|err| format!("failed to write {path}: {err}"))
}

fn render_png(dot: &str, png_path: &str) -> Result<(), String> {
    if let Some(parent) = Path::new(png_path).parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent).map_err(|err| {
                format!("failed to create {}: {err}", parent.display())
            })?;
        }
    }
    let mut child = Command::new("dot")
        .args(["-Tpng", "-o", png_path])
        .stdin(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|err| format!("failed to run dot: {err}"))?;
    let stdin =
        child.stdin.as_mut().ok_or_else(|| "failed to open dot stdin".to_string())?;
    stdin
        .write_all(dot.as_bytes())
        .map_err(|err| format!("failed to write dot input: {err}"))?;
    let output = child
        .wait_with_output()
        .map_err(|err| format!("failed to wait for dot: {err}"))?;
    if !output.status.success() {
        return Err(format!(
            "dot failed: {}",
            String::from_utf8_lossy(&output.stderr).trim()
        ));
    }
    Ok(())
}

fn render_svg(dot: &str) -> Result<String, String> {
    let mut child = Command::new("dot")
        .arg("-Tsvg")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|err| format!("failed to run dot: {err}"))?;
    let stdin =
        child.stdin.as_mut().ok_or_else(|| "failed to open dot stdin".to_string())?;
    stdin
        .write_all(dot.as_bytes())
        .map_err(|err| format!("failed to write dot input: {err}"))?;
    let output = child
        .wait_with_output()
        .map_err(|err| format!("failed to wait for dot: {err}"))?;
    if !output.status.success() {
        return Err(format!(
            "dot failed: {}",
            String::from_utf8_lossy(&output.stderr).trim()
        ));
    }
    String::from_utf8(output.stdout)
        .map_err(|err| format!("dot produced invalid utf8: {err}"))
}

fn wrap_svg_html(svg: &str, details: &str) -> String {
    format!(
        r##"<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KVM IR Graph</title>
<style>
* {{ box-sizing: border-box; }}
body {{
  margin: 0;
  background: #f8fafc;
  color: #111827;
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}}
.toolbar {{
  height: 44px;
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 10px;
  border-bottom: 1px solid #cbd5e1;
  background: rgba(248, 250, 252, 0.95);
}}
.layout {{
  display: grid;
  grid-template-columns: minmax(0, 1fr) 6px minmax(280px, var(--side-width, 380px));
  height: calc(100vh - 44px);
}}
button {{
  border: 1px solid #cbd5e1;
  background: white;
  color: #111827;
  padding: 5px 9px;
  border-radius: 6px;
  cursor: pointer;
}}
.zoom {{ min-width: 52px; text-align: center; color: #64748b; font-size: 12px; }}
.canvas {{
  overflow: hidden;
  background: white;
  cursor: grab;
}}
.canvas.dragging {{ cursor: grabbing; }}
.canvas svg {{
  display: block;
  width: 100%;
  height: 100%;
}}
.resizer {{
  background: #cbd5e1;
  cursor: col-resize;
}}
.resizer:hover {{ background: #94a3b8; }}
.side {{
  background: white;
  border-left: 1px solid #cbd5e1;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}}
.side header {{
  padding: 12px 14px;
  border-bottom: 1px solid #cbd5e1;
}}
.side h1 {{ margin: 0; font-size: 14px; }}
.detail {{
  margin: 0;
  padding: 14px;
  overflow: auto;
  white-space: pre-wrap;
  font: 12px/1.45 Menlo, Consolas, monospace;
}}
.node.selected polygon,
.node.selected ellipse {{
  stroke: #2563eb;
  stroke-width: 3;
}}
@media (max-width: 900px) {{
  .layout {{ grid-template-columns: 1fr; }}
  .resizer {{ display: none; }}
  .side {{ min-height: 240px; border-left: 0; border-top: 1px solid #cbd5e1; }}
}}
</style>
</head>
<body>
<div class="toolbar">
  <button id="fit">Fit</button>
  <button id="zoom-out">-</button>
  <span class="zoom" id="zoom">100%</span>
  <button id="zoom-in">+</button>
  <button id="graph-info">Graph</button>
</div>
<div class="layout">
  <div class="canvas" id="canvas">
  {svg}
  </div>
  <div class="resizer" id="resizer"></div>
  <aside class="side">
    <header><h1 id="title">Inspector</h1></header>
    <pre class="detail" id="detail"></pre>
  </aside>
</div>
<script>
const details = {details};
const svg = document.querySelector('#canvas svg');
const canvas = document.getElementById('canvas');
const zoomReadout = document.getElementById('zoom');
const title = document.getElementById('title');
const detail = document.getElementById('detail');
const originalViewBox = svg.getAttribute('viewBox').split(/\s+/).map(Number);
let base = {{ x: originalViewBox[0], y: originalViewBox[1], width: originalViewBox[2], height: originalViewBox[3] }};
const padX = base.width * 0.04;
const padY = base.height * 0.04;
base = {{ x: base.x - padX, y: base.y - padY, width: base.width + padX * 2, height: base.height + padY * 2 }};
let viewBox = {{ ...base }};
let dragStart = null;
let dragMoved = false;

function updateViewBox() {{
  svg.setAttribute('viewBox', `${{viewBox.x}} ${{viewBox.y}} ${{viewBox.width}} ${{viewBox.height}}`);
  zoomReadout.textContent = `${{Math.round((base.width / viewBox.width) * 100)}}%`;
}}

function zoomAt(factor, clientX, clientY) {{
  const rect = svg.getBoundingClientRect();
  const px = clientX === undefined ? rect.left + rect.width / 2 : clientX;
  const py = clientY === undefined ? rect.top + rect.height / 2 : clientY;
  const sx = (px - rect.left) / rect.width;
  const sy = (py - rect.top) / rect.height;
  const focusX = viewBox.x + sx * viewBox.width;
  const focusY = viewBox.y + sy * viewBox.height;
  const nextWidth = Math.max(base.width * 0.08, Math.min(base.width * 3, viewBox.width / factor));
  const nextHeight = Math.max(base.height * 0.08, Math.min(base.height * 3, viewBox.height / factor));
  viewBox.x = focusX - sx * nextWidth;
  viewBox.y = focusY - sy * nextHeight;
  viewBox.width = nextWidth;
  viewBox.height = nextHeight;
  updateViewBox();
}}

function panByPixels(deltaX, deltaY) {{
  const rect = svg.getBoundingClientRect();
  viewBox.x += deltaX / rect.width * viewBox.width;
  viewBox.y += deltaY / rect.height * viewBox.height;
  updateViewBox();
}}

function wheelDeltaPixels(event) {{
  if (event.deltaMode === WheelEvent.DOM_DELTA_LINE) {{
    return {{ x: event.deltaX * 16, y: event.deltaY * 16 }};
  }}
  if (event.deltaMode === WheelEvent.DOM_DELTA_PAGE) {{
    return {{ x: event.deltaX * canvas.clientWidth, y: event.deltaY * canvas.clientHeight }};
  }}
  return {{ x: event.deltaX, y: event.deltaY }};
}}

function fit() {{
  viewBox = {{ ...base }};
  updateViewBox();
}}

function selectNode(id) {{
  const entry = details[id];
  if (!entry) return;
  document.querySelectorAll('.node.selected').forEach((node) => node.classList.remove('selected'));
  const titleNode = [...svg.querySelectorAll('g.node title')].find((node) => node.textContent === id);
  if (titleNode) titleNode.parentElement.classList.add('selected');
  title.textContent = entry.title;
  detail.textContent = entry.detail;
}}

document.getElementById('fit').addEventListener('click', fit);
document.getElementById('zoom-in').addEventListener('click', () => zoomAt(1.12));
document.getElementById('zoom-out').addEventListener('click', () => zoomAt(1 / 1.12));
canvas.addEventListener('wheel', (event) => {{
  event.preventDefault();
  if (event.ctrlKey) {{
    zoomAt(event.deltaY < 0 ? 1.035 : 1 / 1.035, event.clientX, event.clientY);
    return;
  }}
  const delta = wheelDeltaPixels(event);
  panByPixels(delta.x, delta.y);
}}, {{ passive: false }});
let lastGestureScale = 1;
canvas.addEventListener('gesturestart', (event) => {{
  event.preventDefault();
  lastGestureScale = event.scale || 1;
}});
canvas.addEventListener('gesturechange', (event) => {{
  event.preventDefault();
  const scale = event.scale || 1;
  zoomAt(Math.pow(scale / lastGestureScale, 0.45), event.clientX, event.clientY);
  lastGestureScale = scale;
}});
canvas.addEventListener('gestureend', (event) => {{
  event.preventDefault();
  lastGestureScale = 1;
}});
canvas.addEventListener('pointerdown', (event) => {{
  if (event.target.closest?.('g.node')) {{
    dragMoved = false;
    return;
  }}
  dragStart = {{ x: event.clientX, y: event.clientY, viewBox: {{ ...viewBox }} }};
  dragMoved = false;
  canvas.classList.add('dragging');
  canvas.setPointerCapture(event.pointerId);
}});
canvas.addEventListener('pointermove', (event) => {{
  if (!dragStart) return;
  const rect = svg.getBoundingClientRect();
  if (Math.abs(event.clientX - dragStart.x) + Math.abs(event.clientY - dragStart.y) > 3) {{
    dragMoved = true;
  }}
  viewBox.x = dragStart.viewBox.x - (event.clientX - dragStart.x) / rect.width * dragStart.viewBox.width;
  viewBox.y = dragStart.viewBox.y - (event.clientY - dragStart.y) / rect.height * dragStart.viewBox.height;
  updateViewBox();
}});
canvas.addEventListener('pointerup', (event) => {{
  if (!dragStart) return;
  dragStart = null;
  canvas.classList.remove('dragging');
  if (canvas.hasPointerCapture(event.pointerId)) {{
    canvas.releasePointerCapture(event.pointerId);
  }}
}});
canvas.addEventListener('pointercancel', () => {{
  dragStart = null;
  canvas.classList.remove('dragging');
}});
svg.querySelectorAll('g.node').forEach((group) => {{
  const id = group.querySelector('title')?.textContent;
  if (!id || !details[id]) return;
  group.style.cursor = 'pointer';
  group.addEventListener('click', (event) => {{
    if (dragMoved) return;
    event.stopPropagation();
    selectNode(id);
  }});
}});
document.getElementById('graph-info').addEventListener('click', () => selectNode('graph'));
document.getElementById('resizer').addEventListener('pointerdown', (event) => {{
  const startX = event.clientX;
  const current = parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--side-width')) || 380;
  const onMove = (moveEvent) => {{
    const next = Math.max(280, Math.min(760, current - (moveEvent.clientX - startX)));
    document.documentElement.style.setProperty('--side-width', `${{next}}px`);
  }};
  const onUp = () => {{
    window.removeEventListener('pointermove', onMove);
    window.removeEventListener('pointerup', onUp);
  }};
  window.addEventListener('pointermove', onMove);
  window.addEventListener('pointerup', onUp);
}});
fit();
selectNode('graph');
</script>
</body>
</html>"##
    )
}

fn html_details_js(details: &[GraphHtmlDetail]) -> String {
    let entries = details
        .iter()
        .map(|detail| {
            format!(
                "{}: {{ title: {}, detail: {} }}",
                js_key(&detail.id),
                js_string(&detail.title),
                js_string(&detail.detail)
            )
        })
        .collect::<Vec<_>>()
        .join(", ");
    format!("{{ {entries} }}")
}

fn js_key(input: &str) -> String {
    if input.chars().all(|ch| ch.is_ascii_alphanumeric() || ch == '_') {
        input.to_string()
    } else {
        js_string(input)
    }
}

fn js_escape(input: &str) -> String {
    input
        .replace('\\', "\\\\")
        .replace('\'', "\\'")
        .replace('\n', "\\n")
        .replace('\r', "\\r")
}

fn js_string(input: &str) -> String {
    format!("'{}'", js_escape(input))
}

fn dot_escape(input: &str) -> String {
    input.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n")
}

fn render_err(err: IrError) -> String {
    err.to_string()
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct GraphHtmlDetail {
    id: String,
    title: String,
    detail: String,
}

struct DotStyle {
    fill: &'static str,
    stroke: &'static str,
}

fn value_style(kind: ValueKind, outputs: &BTreeSet<Value>, value: Value) -> DotStyle {
    if outputs.contains(&value) {
        return DotStyle { fill: "#bbf7d0", stroke: "#15803d" };
    }
    match kind {
        ValueKind::Input | ValueKind::Constant => {
            DotStyle { fill: "#dbeafe", stroke: "#1d4ed8" }
        }
        ValueKind::OperationResult => DotStyle { fill: "#f3f4f6", stroke: "#4b5563" },
    }
}
