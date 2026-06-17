// Tests for the external attribute map.
#include <any>
#include <string>

#include "attr.h"
#include "graph.h"
#include "test_harness.h"

using namespace kvm::ir;

namespace {
Type T() { return {"tensor", "hlo"}; }
}  // namespace

KVM_TEST(attr_set_get_on_each_host_kind) {
  Graph g;
  const Value* v = g.MakeInput("v", T());
  Block* root = g.MakeBlock();
  Operator add{"add", "hlo", {T(), T()}, {T()}};
  const Operation* op =
      g.MakeOperation(root, add, {}, {v, v}, {Value{"y", T(), {}}});

  AttrMap attrs;
  attrs.Set(v, "rank", 3)
      .Set(op, "stream", std::string("compute"))
      .Set(static_cast<const Block*>(root), "name", std::string("main"));

  KVM_CHECK(attrs.Has(v, "rank"));
  KVM_CHECK(std::any_cast<int>(*attrs.Get(v, "rank")) == 3);
  KVM_CHECK(std::any_cast<std::string>(*attrs.Get(op, "stream")) == "compute");
  KVM_CHECK(std::any_cast<std::string>(
                *attrs.Get(static_cast<const Block*>(root), "name")) == "main");
}

KVM_TEST(attr_missing_is_null) {
  Graph g;
  const Value* v = g.MakeInput("v", T());
  AttrMap attrs;
  KVM_CHECK(attrs.Get(v, "missing") == nullptr);
  KVM_CHECK(!attrs.Has(v, "missing"));
}

KVM_TEST(attr_remove) {
  Graph g;
  const Value* v = g.MakeInput("v", T());
  AttrMap attrs;
  attrs.Set(v, "k", 1);
  KVM_CHECK(attrs.Remove(v, "k"));
  KVM_CHECK(!attrs.Has(v, "k"));
  KVM_CHECK(!attrs.Remove(v, "k"));  // already gone
}

KVM_TEST(attr_does_not_touch_node) {
  // Attrs are external: the value carries no attr storage of its own.
  Graph g;
  const Value* v = g.MakeInput("v", T());
  AttrMap attrs;
  attrs.Set(v, "k", 1);
  KVM_CHECK(!v->impl.has_value());  // node unchanged by attaching an attr
}

KVM_RUN_ALL()
