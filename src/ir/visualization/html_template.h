#pragma once

// The standalone HTML shell for the visualizer, ported verbatim from the
// KVM_rust render module. RenderHtml fills two slots: the embedded SVG and the
// per-node details object. Kept as raw-string constants so visualizer.cpp stays
// readable. Use: kHtmlHead + svg + kHtmlMid + detailsJs + kHtmlTail.

#include <string_view>

namespace kvm::ir::viz {

inline constexpr std::string_view kHtmlHead = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KVM IR Graph</title>
<style>
* { box-sizing: border-box; }
body {
  margin: 0;
  background: #f8fafc;
  color: #111827;
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}
.toolbar {
  height: 44px;
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 10px;
  border-bottom: 1px solid #cbd5e1;
  background: rgba(248, 250, 252, 0.95);
}
.layout {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 6px minmax(280px, var(--side-width, 380px));
  height: calc(100vh - 44px);
}
button {
  border: 1px solid #cbd5e1;
  background: white;
  color: #111827;
  padding: 5px 9px;
  border-radius: 6px;
  cursor: pointer;
}
.zoom { min-width: 52px; text-align: center; color: #64748b; font-size: 12px; }
.canvas { overflow: hidden; background: white; cursor: grab; }
.canvas.dragging { cursor: grabbing; }
.canvas svg { display: block; width: 100%; height: 100%; }
.resizer { background: #cbd5e1; cursor: col-resize; }
.resizer:hover { background: #94a3b8; }
.side {
  background: white;
  border-left: 1px solid #cbd5e1;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}
.side header { padding: 12px 14px; border-bottom: 1px solid #cbd5e1; }
.side h1 { margin: 0; font-size: 14px; }
.detail {
  margin: 0;
  padding: 14px;
  overflow: auto;
  white-space: pre-wrap;
  font: 12px/1.45 Menlo, Consolas, monospace;
}
.node.selected polygon,
.node.selected ellipse { stroke: #2563eb; stroke-width: 3; }
@media (max-width: 900px) {
  .layout { grid-template-columns: 1fr; }
  .resizer { display: none; }
  .side { min-height: 240px; border-left: 0; border-top: 1px solid #cbd5e1; }
}
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
)HTML";

inline constexpr std::string_view kHtmlMid = R"HTML(
  </div>
  <div class="resizer" id="resizer"></div>
  <aside class="side">
    <header><h1 id="title">Inspector</h1></header>
    <pre class="detail" id="detail"></pre>
  </aside>
</div>
<script>
const details = )HTML";

inline constexpr std::string_view kHtmlTail = R"HTML(;
const svg = document.querySelector('#canvas svg');
const canvas = document.getElementById('canvas');
const zoomReadout = document.getElementById('zoom');
const title = document.getElementById('title');
const detail = document.getElementById('detail');
const originalViewBox = svg.getAttribute('viewBox').split(/\s+/).map(Number);
let base = { x: originalViewBox[0], y: originalViewBox[1], width: originalViewBox[2], height: originalViewBox[3] };
const padX = base.width * 0.04;
const padY = base.height * 0.04;
base = { x: base.x - padX, y: base.y - padY, width: base.width + padX * 2, height: base.height + padY * 2 };
let viewBox = { ...base };
let dragStart = null;
let dragMoved = false;

function updateViewBox() {
  svg.setAttribute('viewBox', `${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`);
  zoomReadout.textContent = `${Math.round((base.width / viewBox.width) * 100)}%`;
}

function zoomAt(factor, clientX, clientY) {
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
}

function panByPixels(deltaX, deltaY) {
  const rect = svg.getBoundingClientRect();
  viewBox.x += deltaX / rect.width * viewBox.width;
  viewBox.y += deltaY / rect.height * viewBox.height;
  updateViewBox();
}

function wheelDeltaPixels(event) {
  if (event.deltaMode === WheelEvent.DOM_DELTA_LINE) {
    return { x: event.deltaX * 16, y: event.deltaY * 16 };
  }
  if (event.deltaMode === WheelEvent.DOM_DELTA_PAGE) {
    return { x: event.deltaX * canvas.clientWidth, y: event.deltaY * canvas.clientHeight };
  }
  return { x: event.deltaX, y: event.deltaY };
}

function fit() {
  viewBox = { ...base };
  updateViewBox();
}

function selectNode(id) {
  const entry = details[id];
  if (!entry) return;
  document.querySelectorAll('.node.selected').forEach((node) => node.classList.remove('selected'));
  const titleNode = [...svg.querySelectorAll('g.node title')].find((node) => node.textContent === id);
  if (titleNode) titleNode.parentElement.classList.add('selected');
  title.textContent = entry.title;
  detail.textContent = entry.detail;
}

document.getElementById('fit').addEventListener('click', fit);
document.getElementById('zoom-in').addEventListener('click', () => zoomAt(1.12));
document.getElementById('zoom-out').addEventListener('click', () => zoomAt(1 / 1.12));
canvas.addEventListener('wheel', (event) => {
  event.preventDefault();
  if (event.ctrlKey) {
    zoomAt(event.deltaY < 0 ? 1.035 : 1 / 1.035, event.clientX, event.clientY);
    return;
  }
  const delta = wheelDeltaPixels(event);
  panByPixels(delta.x, delta.y);
}, { passive: false });
let lastGestureScale = 1;
canvas.addEventListener('gesturestart', (event) => {
  event.preventDefault();
  lastGestureScale = event.scale || 1;
});
canvas.addEventListener('gesturechange', (event) => {
  event.preventDefault();
  const scale = event.scale || 1;
  zoomAt(Math.pow(scale / lastGestureScale, 0.45), event.clientX, event.clientY);
  lastGestureScale = scale;
});
canvas.addEventListener('gestureend', (event) => {
  event.preventDefault();
  lastGestureScale = 1;
});
canvas.addEventListener('pointerdown', (event) => {
  if (event.target.closest?.('g.node')) {
    dragMoved = false;
    return;
  }
  dragStart = { x: event.clientX, y: event.clientY, viewBox: { ...viewBox } };
  dragMoved = false;
  canvas.classList.add('dragging');
  canvas.setPointerCapture(event.pointerId);
});
canvas.addEventListener('pointermove', (event) => {
  if (!dragStart) return;
  const rect = svg.getBoundingClientRect();
  if (Math.abs(event.clientX - dragStart.x) + Math.abs(event.clientY - dragStart.y) > 3) {
    dragMoved = true;
  }
  viewBox.x = dragStart.viewBox.x - (event.clientX - dragStart.x) / rect.width * dragStart.viewBox.width;
  viewBox.y = dragStart.viewBox.y - (event.clientY - dragStart.y) / rect.height * dragStart.viewBox.height;
  updateViewBox();
});
canvas.addEventListener('pointerup', (event) => {
  if (!dragStart) return;
  dragStart = null;
  canvas.classList.remove('dragging');
  if (canvas.hasPointerCapture(event.pointerId)) {
    canvas.releasePointerCapture(event.pointerId);
  }
});
canvas.addEventListener('pointercancel', () => {
  dragStart = null;
  canvas.classList.remove('dragging');
});
svg.querySelectorAll('g.node').forEach((group) => {
  const id = group.querySelector('title')?.textContent;
  if (!id || !details[id]) return;
  group.style.cursor = 'pointer';
  group.addEventListener('click', (event) => {
    if (dragMoved) return;
    event.stopPropagation();
    selectNode(id);
  });
});
document.getElementById('graph-info').addEventListener('click', () => selectNode('graph'));
document.getElementById('resizer').addEventListener('pointerdown', (event) => {
  const startX = event.clientX;
  const current = parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--side-width')) || 380;
  const onMove = (moveEvent) => {
    const next = Math.max(280, Math.min(760, current - (moveEvent.clientX - startX)));
    document.documentElement.style.setProperty('--side-width', `${next}px`);
  };
  const onUp = () => {
    window.removeEventListener('pointermove', onMove);
    window.removeEventListener('pointerup', onUp);
  };
  window.addEventListener('pointermove', onMove);
  window.addEventListener('pointerup', onUp);
});
fit();
selectNode('graph');
</script>
</body>
</html>
)HTML";

}  // namespace kvm::ir::viz
