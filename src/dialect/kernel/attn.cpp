#include "dialect/kernel/attn.h"

#include <string>
#include <string_view>

namespace kvm::ir::kernel {
namespace {

// Grab a top-level `key={...}` or `key=[...]` or `key=word` value from a
// comma-separated field list, respecting [] and {} nesting. Returns the value
// text (without the key=) for `key`, or "" if absent.
std::string_view FieldValue(std::string_view text, std::string_view key) {
  std::size_t i = 0;
  while (i < text.size()) {
    // key
    std::size_t ks = i;
    while (i < text.size() && text[i] != '=') ++i;
    std::string_view k = text.substr(ks, i - ks);
    // trim spaces
    while (!k.empty() && k.front() == ' ') k.remove_prefix(1);
    while (!k.empty() && k.back() == ' ') k.remove_suffix(1);
    if (i < text.size()) ++i;  // '='
    // value up to top-level comma
    std::size_t vs = i;
    int depth = 0;
    for (; i < text.size(); ++i) {
      char c = text[i];
      if (c == '[' || c == '{')
        ++depth;
      else if (c == ']' || c == '}')
        --depth;
      else if (c == ',' && depth == 0)
        break;
    }
    std::string_view v = text.substr(vs, i - vs);
    while (!v.empty() && v.front() == ' ') v.remove_prefix(1);
    while (!v.empty() && v.back() == ' ') v.remove_suffix(1);
    if (i < text.size()) ++i;  // ','
    if (k == key) return v;
  }
  return {};
}

std::string FormatBatch(const AttnBatchDesc& b) {
  return "{off=" + text::FormatInts(b.offsets) +
         ",pos=" + text::FormatInts(b.positions) + "}";
}

// Strip a single layer of surrounding {} so inner key=value pairs parse with a
// clean first key (the leading '{' would otherwise glue onto the first key).
std::string_view Unbrace(std::string_view s) {
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  if (s.size() >= 2 && s.front() == '{' && s.back() == '}') {
    s.remove_prefix(1);
    s.remove_suffix(1);
  }
  return s;
}

AttnBatchDesc ParseBatch(std::string_view s) {
  s = Unbrace(s);
  AttnBatchDesc b;
  b.offsets = text::ParseInts(FieldValue(s, "off"));
  b.positions = text::ParseInts(FieldValue(s, "pos"));
  return b;
}

}  // namespace

std::string AttnMetaImpl::Serialize() const {
  std::string out = "q=" + FormatBatch(q_batch);
  out += ", kv={off=" + text::FormatInts(kv_batch.desc.offsets);
  out += ",pos=" + text::FormatInts(kv_batch.desc.positions);
  out += ",blocks=" + text::FormatIntLists(kv_batch.block_table) + "}";
  out += ", mask=" + MaskName(mask);
  return out;
}

AttnMetaImpl AttnMetaImpl::Deserialize(std::string_view text) {
  AttnMetaImpl m;
  m.q_batch = ParseBatch(FieldValue(text, "q"));
  std::string_view kv = Unbrace(FieldValue(text, "kv"));
  m.kv_batch.desc = ParseBatch(kv);
  m.kv_batch.block_table = text::ParseIntLists(FieldValue(kv, "blocks"));
  m.mask = MaskFromName(FieldValue(text, "mask"));
  return m;
}

}  // namespace kvm::ir::kernel
