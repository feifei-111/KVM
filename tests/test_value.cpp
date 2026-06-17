// Tests for the open-but-bounded mark (AnyOf) and builtin value impls.
#include <concepts>
#include <vector>

#include "builtin.h"
#include "test_harness.h"
#include "value.h"

using namespace kvm::ir;

// An out-of-core "dialect" impl, registered into the ValueImpl mark in one
// line. The core never names this type -- this is the proof of openness.
namespace demo {
struct TensorValueImpl {
  std::vector<int> shape;
};
}  // namespace demo
KVM_MARK_MEMBER(::kvm::ir::ValueImpl, ::demo::TensorValueImpl);

// Bounded: registered members are storable; unregistered ones are not.
static_assert(std::constructible_from<AnyOf<ValueImpl>, ConstIntImpl>);
static_assert(std::constructible_from<AnyOf<ValueImpl>, demo::TensorValueImpl>);
static_assert(!std::constructible_from<AnyOf<ValueImpl>, int>);
static_assert(!std::constructible_from<AnyOf<ValueImpl>, std::string>);

KVM_TEST(builtin_const_store_and_recover) {
  Value b{"b", {"bool", "builtin"}, ConstBoolImpl{true}};
  Value i{"i", {"int", "builtin"}, ConstIntImpl{42}};
  Value f{"f", {"float", "builtin"}, ConstFloatImpl{3.5}};

  KVM_CHECK(b.impl.as<ConstBoolImpl>() != nullptr);
  KVM_CHECK(b.impl.as<ConstBoolImpl>()->value == true);
  KVM_CHECK(i.impl.as<ConstIntImpl>()->value == 42);
  KVM_CHECK(f.impl.as<ConstFloatImpl>()->value == 3.5);
}

KVM_TEST(tagless_wrong_member_is_null) {
  Value i{"i", {"int", "builtin"}, ConstIntImpl{42}};
  // No tag: asking for the wrong member just fails (nullptr), not throws.
  KVM_CHECK(i.impl.as<ConstBoolImpl>() == nullptr);
  KVM_CHECK(i.impl.as<ConstFloatImpl>() == nullptr);
}

KVM_TEST(empty_impl_has_no_value) {
  Value v{"v", {"opaque", "x"}, {}};
  KVM_CHECK(!v.impl.has_value());
  KVM_CHECK(v.impl.as<ConstIntImpl>() == nullptr);
}

KVM_TEST(open_extension_out_of_core) {
  Value t{"t", {"tensor", "demo"}, demo::TensorValueImpl{{8, 4096}}};
  const auto* tv = t.impl.as<demo::TensorValueImpl>();
  KVM_CHECK(tv != nullptr);
  KVM_CHECK(tv->shape.size() == 2);
  KVM_CHECK(tv->shape[1] == 4096);
}

KVM_TEST(impl_recovered_only_inside_a_match_branch) {
  // There is no unified "GetImpl(type) -> Some": a sum has no member outside a
  // match. Recovery is a single-branch probe with as<T>(); inside the branch
  // the type is concrete. The branch you take is your own logic, not a unified
  // operation returning a unified result.
  Value i{"i", {"int", "builtin"}, ConstIntImpl{42}};
  if (const ConstIntImpl* c = i.impl.as<ConstIntImpl>()) {
    KVM_CHECK(c->value == 42);
  } else {
    KVM_CHECK(false);  // should have taken the int branch
  }
}

KVM_RUN_ALL()
