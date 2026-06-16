#include "serialization/serializer.h"

#include <any>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "builtin.h"  // BlockOpImpl lives in graph.h; builtin pulled for completeness

namespace kvm::ir::serial {
namespace {

// Render a JSON-like scalar attr value. Unknown types are skipped (return
// false). Attrs are dynamic pass data; the text form need not be lossless.
bool RenderAttrValue(const std::any& v, std::string& out) {
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
    out += '"';
    out += *p;
    out += '"';
    return true;
  }
  return false;
}

std::string RenderType(const Type& t) {
  return t.dialect.empty() ? t.name : t.dialect + "." + t.name;
}

class Serializer {
 public:
  Serializer(const Graph& g, const Registry& reg, const AttrMap* attrs)
      : g_(g), reg_(reg), attrs_(attrs) {}

  std::string Run() {
    out_ = "graph {\n";
    RenderConfig();
    if (g_.main()) RenderBlock(g_.main(), 1);
    out_ += "}\n";
    return out_;
  }

 private:
  void Indent(int n) { out_.append(n * 2, ' '); }

  void RenderConfig() {
    const auto& ranks = g_.config().dist.all_ranks;
    if (ranks.empty()) return;
    Indent(1);
    out_ += "config { ranks = [";
    for (std::size_t i = 0; i < ranks.size(); ++i) {
      if (i) out_ += ", ";
      out_ += std::to_string(ranks[i]);
    }
    out_ += "] }\n";
  }

  // `{attr}` for a host, or "" if no (renderable) attrs.
  template <class Host>
  std::string RenderAttrs(Host host) {
    if (!attrs_) return "";
    const Attr* bag = attrs_->GetAttr(host);
    if (!bag || bag->empty()) return "";
    std::string body;
    bool first = true;
    for (const auto& [k, v] : *bag) {
      std::string piece = k + " = ";
      std::string val;
      if (!RenderAttrValue(v, val)) continue;  // skip unrenderable
      piece += val;
      if (!first) body += ", ";
      body += piece;
      first = false;
    }
    if (body.empty()) return "";
    return " {" + body + "}";
  }

  // `%name: type<impl> {attr}` for a value definition.
  std::string RenderValueDef(const Value* v) {
    std::string s = "%" + v->name + ": " + RenderType(v->type);
    if (v->impl.has_value()) {
      auto inner = reg_.SerializeValueImpl(KeyOf(v->type), v->impl);
      if (inner && !inner->empty()) s += "<" + *inner + ">";
    }
    s += RenderAttrs(v);
    return s;
  }

  void RenderBlock(const Block* b, int depth) {
    Indent(depth);
    out_ += "^" + b->name + "(";
    for (std::size_t i = 0; i < b->inputs.size(); ++i) {
      if (i) out_ += ", ";
      out_ += RenderValueDef(b->inputs[i]);
    }
    out_ += ") {\n";
    for (const Operation* op : b->operations) RenderOperation(op, depth + 1);
    if (!b->outputs.empty()) {
      Indent(depth + 1);
      out_ += "return";
      for (std::size_t i = 0; i < b->outputs.size(); ++i) {
        out_ += i ? ", " : " ";
        out_ += "%" + b->outputs[i]->name;
      }
      out_ += "\n";
    }
    Indent(depth);
    out_ += "}\n";
  }

  void RenderOperation(const Operation* op, int depth) {
    Indent(depth);
    auto outs = g_.GetOutputs(op);
    if (!outs.empty()) {
      for (std::size_t i = 0; i < outs.size(); ++i) {
        if (i) out_ += ", ";
        out_ += RenderValueDef(outs[i]);
      }
      out_ += " = ";
    }
    out_ += KeyOf(op->op);
    RenderOpImpl(op, depth);
    out_ += RenderAttrs(op);
    // operands
    out_ += "(";
    auto ins = g_.GetInputs(op);
    for (std::size_t i = 0; i < ins.size(); ++i) {
      if (i) out_ += ", ";
      out_ += "%" + ins[i]->name;
    }
    out_ += ")\n";
  }

  // `<impl>`: leaf impls via registry; BlockOpImpl recurses into its block.
  void RenderOpImpl(const Operation* op, int depth) {
    if (!op->impl.has_value()) return;
    if (const BlockOpImpl* bo = op->impl.as<BlockOpImpl>()) {
      // nested block as the op's impl
      out_ += "<\n";
      if (bo->block) RenderBlock(bo->block, depth + 1);
      Indent(depth);
      out_ += ">";
      return;
    }
    auto inner = reg_.SerializeOperationImpl(KeyOf(op->op), op->impl);
    if (inner && !inner->empty()) out_ += "<" + *inner + ">";
  }

  const Graph& g_;
  const Registry& reg_;
  const AttrMap* attrs_;
  std::string out_;
};

}  // namespace

std::string Serialize(const Graph& graph, const Registry& registry,
                      const AttrMap* attrs) {
  return Serializer(graph, registry, attrs).Run();
}

}  // namespace kvm::ir::serial
