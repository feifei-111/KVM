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
    if (g_.root()) RenderBlock(g_.root(), 1);
    out_ += "}\n";
    return out_;
  }

 private:
  void Indent(int n) { out_.append(n * 2, ' '); }

  void RenderConfig() {
    const GraphConfig& cfg = g_.config();
    const auto& ranks = cfg.dist.all_ranks;
    const KVMeta& kv = cfg.kv;
    bool has_kv = kv.layer_size || kv.block_size || kv.token_bytes ||
                  kv.layout != KVLayout::kLayerFirst;
    if (ranks.empty() && !has_kv) return;

    Indent(1);
    out_ += "config {";
    bool first = true;
    auto sep = [&] {
      out_ += first ? " " : ", ";
      first = false;
    };
    if (!ranks.empty()) {
      sep();
      out_ += "ranks = [";
      for (std::size_t i = 0; i < ranks.size(); ++i) {
        if (i) out_ += ", ";
        out_ += std::to_string(ranks[i]);
      }
      out_ += "]";
    }
    if (has_kv) {
      sep();
      out_ += "kv = {layout=" + KVLayoutName(kv.layout) +
              ", layer_size=" + std::to_string(kv.layer_size) +
              ", block_size=" + std::to_string(kv.block_size) +
              ", token_bytes=" + std::to_string(kv.token_bytes) + "}";
    }
    out_ += " }\n";
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
  std::string RenderValueDef(const ValueNode* vn) {
    const Value& v = vn->value();
    std::string s = "%" + v.name + ": " + RenderType(v.type);
    if (v.impl.has_value()) {
      auto inner = reg_.SerializeValueImpl(KeyOf(v.type), v.impl);
      if (inner && !inner->empty()) s += "<" + *inner + ">";
    }
    s += RenderAttrs(vn);
    return s;
  }

  void RenderBlock(const Block* b, int depth) {
    Indent(depth);
    out_ += "^" + b->name() + "(";
    const auto& args = b->arguments();
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (i) out_ += ", ";
      out_ += RenderValueDef(args[i]);
    }
    out_ += ") {\n";
    for (const OpNode* op : b->operations()) RenderOperation(op, depth + 1);
    const auto& outs = b->outputs();
    if (!outs.empty()) {
      Indent(depth + 1);
      out_ += "return";
      for (std::size_t i = 0; i < outs.size(); ++i) {
        out_ += i ? ", " : " ";
        out_ += "%" + outs[i]->value().name;
      }
      out_ += "\n";
    }
    Indent(depth);
    out_ += "}\n";
  }

  void RenderOperation(const OpNode* op, int depth) {
    Indent(depth);
    const auto& outs = op->results();
    if (!outs.empty()) {
      for (std::size_t i = 0; i < outs.size(); ++i) {
        if (i) out_ += ", ";
        out_ += RenderValueDef(outs[i]);
      }
      out_ += " = ";
    }
    out_ += KeyOf(op->op().op);
    RenderOpImpl(op, depth);
    out_ += RenderAttrs(op);
    // operands
    out_ += "(";
    const auto& ins = op->operands();
    for (std::size_t i = 0; i < ins.size(); ++i) {
      if (i) out_ += ", ";
      out_ += "%" + ins[i]->value().name;
    }
    out_ += ")\n";
  }

  // `<impl>`: leaf impls via registry; BlockOpImpl recurses into its block.
  void RenderOpImpl(const OpNode* op, int depth) {
    const Operation& payload = op->op();
    if (!payload.impl.has_value()) return;
    if (const BlockOpImpl* bo = payload.impl.as<BlockOpImpl>()) {
      // nested block as the op's impl
      out_ += "<\n";
      if (bo->block) RenderBlock(bo->block, depth + 1);
      Indent(depth);
      out_ += ">";
      return;
    }
    auto inner = reg_.SerializeOperationImpl(KeyOf(payload.op), payload.impl);
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
