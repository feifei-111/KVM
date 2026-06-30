#include "serialization/parser.h"

#include <glog/logging.h>

#include <any>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kvm::ir::serial {
namespace {

// A cursor over the input with small primitives. The grammar is exactly what
// serializer.cpp prints, so a hand cursor beats a separate lexer.
class Cursor {
 public:
  explicit Cursor(std::string_view s) : s_(s) {}

  bool Eof() const { return pos_ >= s_.size(); }
  char Peek() const { return Eof() ? '\0' : s_[pos_]; }
  std::size_t pos() const { return pos_; }
  void seek(std::size_t p) { pos_ = p; }

  void SkipWs() {
    while (!Eof() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
  }

  void Expect(char c) {
    SkipWs();
    CHECK(Peek() == c) << "parse: expected '" << c
                       << "' near: " << s_.substr(pos_, 24);
    ++pos_;
  }

  bool Accept(char c) {
    SkipWs();
    if (Peek() == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  static bool IsIdent(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
  }

  bool AcceptWord(std::string_view w) {
    SkipWs();
    if (s_.substr(pos_, w.size()) == w) {
      std::size_t after = pos_ + w.size();
      char n = after < s_.size() ? s_[after] : '\0';
      if (!IsIdent(n)) {
        pos_ = after;
        return true;
      }
    }
    return false;
  }

  bool PeekChar(char c) {
    SkipWs();
    return Peek() == c;
  }

  std::string Ident() {
    SkipWs();
    std::size_t start = pos_;
    while (!Eof() && IsIdent(s_[pos_])) ++pos_;
    CHECK(pos_ != start) << "parse: expected identifier near: "
                         << s_.substr(pos_, 24);
    return std::string(s_.substr(start, pos_ - start));
  }

  // Text up to (not including) any char in `stop`, respecting <> {} [] nesting
  // and quoted strings. Trims trailing whitespace.
  std::string Until(std::string_view stop) {
    SkipWs();
    std::size_t start = pos_;
    int angle = 0, brace = 0, brack = 0;
    bool in_str = false;
    while (!Eof()) {
      char c = s_[pos_];
      if (in_str) {
        if (c == '\\') {
          pos_ += 2;
          continue;
        }
        if (c == '"') in_str = false;
        ++pos_;
        continue;
      }
      if (c == '"') {
        in_str = true;
        ++pos_;
        continue;
      }
      if (angle == 0 && brace == 0 && brack == 0 &&
          stop.find(c) != std::string_view::npos) {
        break;
      }
      if (c == '<')
        ++angle;
      else if (c == '>')
        --angle;
      else if (c == '{')
        ++brace;
      else if (c == '}')
        --brace;
      else if (c == '[')
        ++brack;
      else if (c == ']')
        --brack;
      ++pos_;
    }
    std::size_t end = pos_;
    while (end > start &&
           std::isspace(static_cast<unsigned char>(s_[end - 1]))) {
      --end;
    }
    return std::string(s_.substr(start, end - start));
  }

 private:
  std::string_view s_;
  std::size_t pos_ = 0;
};

std::any ParseAttrScalar(const std::string& tok) {
  if (tok == "true") return std::any(true);
  if (tok == "false") return std::any(false);
  if (!tok.empty() && tok.front() == '"' && tok.back() == '"') {
    return std::any(tok.substr(1, tok.size() - 2));
  }
  if (tok.find('.') != std::string::npos ||
      tok.find('e') != std::string::npos ||
      tok.find('E') != std::string::npos) {
    return std::any(std::stod(tok));
  }
  return std::any(static_cast<std::int64_t>(std::stoll(tok)));
}

Type ParseType(const std::string& s) {
  auto dot = s.rfind('.');
  if (dot == std::string::npos) return Type{s, ""};
  return Type{s.substr(dot + 1), s.substr(0, dot)};
}

using AttrPairs = std::vector<std::pair<std::string, std::any>>;

// One parsed value definition `%name: type<impl>` plus any trailing attrs.
struct ValueSpec {
  std::string name;
  Type type;
  AnyOf<ValueImpl> impl;
  AttrPairs attrs;
};

// Recursive-descent parser; grammar mirrors serializer.cpp exactly.
class Parser {
 public:
  Parser(std::string_view text, const Registry& reg, Graph& g)
      : cur_(text), reg_(reg), g_(g) {}

  AttrMap Run() {
    CHECK(cur_.AcceptWord("graph")) << "parse: expected 'graph'";
    cur_.Expect('{');
    if (PeekWord("config")) ParseConfig();
    Block* root = ParseBlock();
    g_.SetRoot(root);
    cur_.Expect('}');
    return std::move(attrs_);
  }

 private:
  bool PeekWord(std::string_view w) {
    std::size_t save = cur_.pos();
    bool ok = cur_.AcceptWord(w);
    cur_.seek(save);
    return ok;
  }

  void ParseConfig() {
    cur_.AcceptWord("config");
    cur_.Expect('{');
    if (cur_.Accept('}')) return;
    do {
      std::string key = cur_.Ident();
      cur_.Expect('=');
      if (key == "ranks") {
        cur_.Expect('[');
        std::vector<int> ranks;
        if (!cur_.Accept(']')) {
          do {
            ranks.push_back(std::stoi(cur_.Until(",]")));
          } while (cur_.Accept(','));
          cur_.Expect(']');
        }
        g_.config().dist.all_ranks = std::move(ranks);
      } else if (key == "kv") {
        ParseKVMeta();
      } else {
        cur_.Until(",}");  // unknown config key: skip its value
      }
    } while (cur_.Accept(','));
    cur_.Expect('}');
  }

  // kv = {layout=page_first, layer_size=.., block_size=.., token_bytes=..}
  void ParseKVMeta() {
    cur_.Expect('{');
    KVMeta kv;
    if (!cur_.Accept('}')) {
      do {
        std::string k = cur_.Ident();
        cur_.Expect('=');
        std::string v = cur_.Until(",}");
        if (k == "layout")
          kv.layout = KVLayoutFromName(v);
        else if (k == "layer_size")
          kv.layer_size = std::stoi(v);
        else if (k == "block_size")
          kv.block_size = std::stoi(v);
        else if (k == "token_bytes")
          kv.token_bytes = std::stoi(v);
      } while (cur_.Accept(','));
      cur_.Expect('}');
    }
    g_.config().kv = kv;
  }

  // `{k = v, ...}` -> pairs; empty if no '{'.
  AttrPairs ParseAttrs() {
    AttrPairs pairs;
    if (!cur_.Accept('{')) return pairs;
    if (cur_.Accept('}')) return pairs;
    do {
      std::string key = cur_.Ident();
      cur_.Expect('=');
      pairs.emplace_back(std::move(key), ParseAttrScalar(cur_.Until(",}")));
    } while (cur_.Accept(','));
    cur_.Expect('}');
    return pairs;
  }

  // `%name: dialect.type<impl> {attr}` (the impl/attr are optional).
  ValueSpec ParseValueSpec() {
    cur_.Expect('%');
    ValueSpec spec;
    spec.name = cur_.Ident();
    cur_.Expect(':');
    spec.type = ParseType(cur_.Ident());
    if (cur_.Accept('<')) {
      std::string body = cur_.Until(">");
      if (auto v = reg_.DeserializeValueImpl(KeyOf(spec.type), body)) {
        spec.impl = std::move(*v);
      }
      cur_.Expect('>');
    }
    spec.attrs = ParseAttrs();
    return spec;
  }

  ValueNode* ResolveRef() {
    cur_.Expect('%');
    std::string name = cur_.Ident();
    auto it = symbols_.find(name);
    CHECK(it != symbols_.end()) << "parse: undefined value %" << name;
    return it->second;
  }

  template <class Host>
  void AttachAttrs(Host host, const AttrPairs& pairs) {
    for (const auto& [k, v] : pairs) attrs_.Set(host, k, v);
  }

  // ^name(%a: t, ...) { ops... return %x, ... }
  Block* ParseBlock() {
    cur_.Expect('^');
    std::string block_name = cur_.Ident();
    Block* block = g_.arena().NewBlock(block_name);

    // header arguments: created now so operands inside can reference them.
    cur_.Expect('(');
    if (!cur_.Accept(')')) {
      do {
        ValueSpec s = ParseValueSpec();
        ValueNode* v =
            block->AddArgument(Value{s.name, s.type, std::move(s.impl)});
        symbols_[s.name] = v;
        AttachAttrs(v, s.attrs);
      } while (cur_.Accept(','));
      cur_.Expect(')');
    }

    cur_.Expect('{');
    std::vector<ValueNode*> outputs;
    while (!cur_.PeekChar('}')) {
      if (PeekWord("return")) {
        cur_.AcceptWord("return");
        do {
          outputs.push_back(ResolveRef());
        } while (cur_.Accept(','));
        break;
      }
      ParseOperation(block);
    }
    if (!outputs.empty()) block->SetOutputs(std::move(outputs));
    cur_.Expect('}');
    return block;
  }

  // [<outs> =] key<impl> {attr} (operands)
  void ParseOperation(Block* block) {
    std::vector<ValueSpec> out_specs;
    if (cur_.PeekChar('%')) {
      do {
        out_specs.push_back(ParseValueSpec());
      } while (cur_.Accept(','));
      cur_.Expect('=');
    }

    std::string key = cur_.Ident();
    auto found = reg_.GetOperator(key);
    CHECK(found) << "parse: unknown operator '" << key
                 << "': no dialect has registered it";
    Operator op = std::move(*found);

    AnyOf<OperationImpl> impl;
    if (cur_.Accept('<')) {
      if (cur_.PeekChar('^')) {
        Block* child = ParseBlock();  // nested block-op
        impl = BlockOpImpl{child};
      } else {
        std::string body = cur_.Until(">");
        if (auto v = reg_.DeserializeOperationImpl(key, body)) {
          impl = std::move(*v);
        }
      }
      cur_.Expect('>');
    }

    AttrPairs op_attrs = ParseAttrs();

    std::vector<ValueNode*> operands;
    cur_.Expect('(');
    if (!cur_.Accept(')')) {
      do {
        operands.push_back(ResolveRef());
      } while (cur_.Accept(','));
      cur_.Expect(')');
    }

    std::vector<Value> outs;
    outs.reserve(out_specs.size());
    for (auto& s : out_specs) {
      outs.push_back(Value{s.name, s.type, std::move(s.impl)});
    }

    OpNode* operation =
        block->MakeOperation(Operation{std::move(op), std::move(impl)},
                             std::move(operands), std::move(outs));
    AttachAttrs(operation, op_attrs);

    const auto& created = operation->results();
    for (std::size_t i = 0; i < created.size(); ++i) {
      symbols_[out_specs[i].name] = created[i];
      AttachAttrs(created[i], out_specs[i].attrs);
    }
  }

  Cursor cur_;
  const Registry& reg_;
  Graph& g_;
  AttrMap attrs_;
  std::unordered_map<std::string, ValueNode*> symbols_;
};

}  // namespace

AttrMap Parse(const std::string& text, const Registry& registry, Graph& graph) {
  return Parser(text, registry, graph).Run();
}

}  // namespace kvm::ir::serial
