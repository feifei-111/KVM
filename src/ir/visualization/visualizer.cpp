#include "visualization/visualizer.h"

#include <glog/logging.h>
#include <unistd.h>

#include <any>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "visualization/html_template.h"

namespace kvm::ir::viz {
namespace {

// Stable string ids for nodes. Values and operations get sequential indices in
// first-seen order, yielding "value0/op0/..." -- matching the Rust id scheme
// and the SVG <title> lookup keys.
class Ids {
 public:
  std::string ValueId(const ValueNode* v) {
    return "value" + std::to_string(Index(value_, v));
  }
  std::string OpId(const OpNode* o) {
    return "op" + std::to_string(Index(op_, o));
  }

 private:
  template <class T>
  std::size_t Index(std::unordered_map<const T*, std::size_t>& m, const T* p) {
    auto [it, inserted] = m.try_emplace(p, m.size());
    return it->second;
  }
  std::unordered_map<const ValueNode*, std::size_t> value_;
  std::unordered_map<const OpNode*, std::size_t> op_;
};

std::string DotEscape(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '\\' || c == '"') out += '\\';
    if (c == '\n') {
      out += "\\n";
      continue;
    }
    out += c;
  }
  return out;
}

std::string TypeText(const Type& t) {
  if (t.dialect.empty() || t.name.empty()) return t.name;
  return t.dialect + "." + t.name;
}

std::string OperatorText(const Operator& op) {
  if (op.dialect.empty() || op.name.empty()) return op.name;
  return op.dialect + "." + op.name;
}

struct Style {
  const char* fill;
  const char* stroke;
};

// Value coloring: block outputs green, level inputs (no def) blue, op results
// gray -- matching the Rust palette.
Style ValueStyle(const ValueNode* v,
                 const std::unordered_set<const ValueNode*>& outputs) {
  if (outputs.count(v)) return {"#bbf7d0", "#15803d"};
  if (v->def() == nullptr) return {"#dbeafe", "#1d4ed8"};
  return {"#f3f4f6", "#4b5563"};
}

void EmitValue(Ids& ids, std::string& out,
               const std::unordered_set<const ValueNode*>& outputs,
               std::unordered_set<const ValueNode*>& seen, const ValueNode* v) {
  if (!seen.insert(v).second) return;
  Style s = ValueStyle(v, outputs);
  out += "  " + ids.ValueId(v) + " [label=\"" +
         DotEscape("%" + v->value().name) +
         "\", shape=box, style=\"filled\", fillcolor=\"" + s.fill +
         "\", color=\"" + s.stroke + "\"];\n";
}

}  // namespace

std::string RenderDot(const Graph& g) {
  Ids ids;
  std::string out = "digraph ir {\n";
  out += "  graph [rankdir=TB, ranksep=0.85, nodesep=0.35];\n";
  out += "  node [fontname=\"Menlo\"];\n";
  out += "  edge [fontname=\"Menlo\"];\n";

  const Block* block = g.root();
  std::unordered_set<const ValueNode*> outputs;
  if (block) {
    for (const ValueNode* v : block->outputs()) outputs.insert(v);
  }

  std::unordered_set<const ValueNode*> seen;
  if (block) {
    for (const ValueNode* v : block->arguments())
      EmitValue(ids, out, outputs, seen, v);
    for (const ValueNode* v : block->outputs())
      EmitValue(ids, out, outputs, seen, v);
    for (const OpNode* op : block->operations()) {
      out += "  " + ids.OpId(op) + " [label=\"" +
             DotEscape(OperatorText(op->op().op)) +
             "\", shape=ellipse, style=\"filled\", fillcolor=\"#fde68a\", "
             "color=\"#b45309\"];\n";
      for (const ValueNode* operand : op->operands()) {
        EmitValue(ids, out, outputs, seen, operand);
        out += "  " + ids.ValueId(operand) + " -> " + ids.OpId(op) + ";\n";
      }
      for (const ValueNode* result : op->results()) {
        EmitValue(ids, out, outputs, seen, result);
        out += "  " + ids.OpId(op) + " -> " + ids.ValueId(result) + ";\n";
      }
    }
  }
  out += "}";
  return out;
}

namespace {

// Run `dot -Tsvg`, feeding `dot_text` on stdin, returning the SVG on stdout.
std::string RunDotToSvg(const std::string& dot_text) {
  std::string tmpl = "/tmp/kvm_viz_XXXXXX";
  std::vector<char> in_path(tmpl.begin(), tmpl.end());
  in_path.push_back('\0');
  int fd = ::mkstemp(in_path.data());
  CHECK(fd >= 0) << "visualizer: cannot create temp file";
  ::close(fd);
  std::string in(in_path.data());
  std::string svg_path = in + ".svg";

  {
    FILE* f = std::fopen(in.c_str(), "wb");
    CHECK(f) << "visualizer: cannot write temp dot";
    std::fwrite(dot_text.data(), 1, dot_text.size(), f);
    std::fclose(f);
  }

  std::string cmd = "dot -Tsvg '" + in + "' -o '" + svg_path + "' 2>/dev/null";
  int rc = std::system(cmd.c_str());
  if (rc != 0) std::remove(in.c_str());
  CHECK(rc == 0) << "visualizer: `dot` failed (is Graphviz installed and on "
                    "PATH?)";

  std::string svg;
  if (FILE* f = std::fopen(svg_path.c_str(), "rb")) {
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) svg.append(buf, n);
    std::fclose(f);
  }
  std::remove(in.c_str());
  std::remove(svg_path.c_str());
  return svg;
}

std::string JsEscape(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '\'':
        out += "\\'";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        out += c;
    }
  }
  return out;
}

std::string JsString(const std::string& s) { return "'" + JsEscape(s) + "'"; }

std::string JsKey(const std::string& s) {
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_';
    if (!ok) return JsString(s);
  }
  return s;
}

// JSON-like rendering of one attr value (same scalar set as the serializer).
bool RenderAttrScalar(const std::any& v, std::string& out) {
  if (auto p = std::any_cast<bool>(&v)) {
    out += *p ? "true" : "false";
    return true;
  }
  if (auto p = std::any_cast<int>(&v)) {
    out += std::to_string(*p);
    return true;
  }
  if (auto p = std::any_cast<std::int64_t>(&v)) {
    out += std::to_string(*p);
    return true;
  }
  if (auto p = std::any_cast<double>(&v)) {
    out += std::to_string(*p);
    return true;
  }
  if (auto p = std::any_cast<std::string>(&v)) {
    out += "\"" + *p + "\"";
    return true;
  }
  return false;
}

// A host's attrs as pretty indented "key: value" lines, or "{}" if none.
std::string AttrText(const AttrMap* attrs, AttrHost host) {
  if (!attrs) return "{}";
  const Attr* bag = attrs->GetAttr(host);
  if (!bag || bag->empty()) return "{}";
  std::string out = "{\n";
  for (const auto& [k, v] : *bag) {
    std::string val;
    if (!RenderAttrScalar(v, val)) continue;
    out += "  " + k + ": " + val + "\n";
  }
  out += "}";
  return out;
}

// One inspector entry: id (matches the SVG <title>), title, detail text.
struct Detail {
  std::string id;
  std::string title;
  std::string text;
};

std::string ValueName(const ValueNode* v) { return "%" + v->value().name; }

void PushValueDetail(Ids& ids, const AttrMap* attrs,
                     std::unordered_set<const ValueNode*>& seen,
                     const ValueNode* v, std::vector<Detail>& out) {
  if (!seen.insert(v).second) return;
  std::string text = "name: " + ValueName(v) + "\n" +
                     "type: " + TypeText(v->value().type) + "\n" + "attrs:\n" +
                     AttrText(attrs, v);
  out.push_back({ids.ValueId(v), ValueName(v), std::move(text)});
}

std::string ValueList(const std::vector<ValueNode*>& vs) {
  if (vs.empty()) return "()";
  std::string s;
  for (std::size_t i = 0; i < vs.size(); ++i) {
    if (i) s += ", ";
    s += ValueName(vs[i]);
  }
  return s;
}

std::vector<Detail> BuildDetails(const Graph& g, Ids& ids,
                                 const AttrMap* attrs) {
  std::vector<Detail> details;
  details.push_back({"graph", "Graph", "attrs:\n{}"});

  const Block* block = g.root();
  if (!block) return details;

  std::unordered_set<const ValueNode*> seen;
  for (const ValueNode* v : block->arguments())
    PushValueDetail(ids, attrs, seen, v, details);
  for (const ValueNode* v : block->outputs())
    PushValueDetail(ids, attrs, seen, v, details);

  for (const OpNode* op : block->operations()) {
    const std::string op_text = OperatorText(op->op().op);
    std::string text = "operation: " + op_text + "\n" +
                       "inputs: " + ValueList(op->operands()) + "\n" +
                       "outputs: " + ValueList(op->results()) + "\n" +
                       "attrs:\n" + AttrText(attrs, op);
    details.push_back({ids.OpId(op), op_text, std::move(text)});
    for (const ValueNode* v : op->operands())
      PushValueDetail(ids, attrs, seen, v, details);
    for (const ValueNode* v : op->results())
      PushValueDetail(ids, attrs, seen, v, details);
  }
  return details;
}

std::string DetailsJs(const std::vector<Detail>& details) {
  std::string out = "{ ";
  for (std::size_t i = 0; i < details.size(); ++i) {
    if (i) out += ", ";
    out += JsKey(details[i].id) + ": { title: " + JsString(details[i].title) +
           ", detail: " + JsString(details[i].text) + " }";
  }
  out += " }";
  return out;
}

}  // namespace

std::string RenderHtml(const Graph& g, const AttrMap* attrs) {
  Ids ids;
  std::string dot = RenderDot(g);  // its own Ids, but ids are deterministic
  // Rebuild details with a fresh Ids that reproduces the same value0/op0 order.
  // RenderDot and BuildDetails visit nodes in the same order (block inputs,
  // outputs, then ops with operands/results), so the ids line up with the SVG.
  std::string svg = RunDotToSvg(dot);
  std::vector<Detail> details = BuildDetails(g, ids, attrs);

  std::string out;
  out.reserve(kHtmlHead.size() + svg.size() + kHtmlTail.size() + 1024);
  out.append(kHtmlHead);
  out.append(svg);
  out.append(kHtmlMid);
  out.append(DetailsJs(details));
  out.append(kHtmlTail);
  return out;
}

}  // namespace kvm::ir::viz
