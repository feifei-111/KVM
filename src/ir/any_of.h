#pragma once

// AnyOf<Tag>: an OPEN but BOUNDED sum type. This IS the sum itself -- e.g.
// ValueImpl = AnyOf<ValueImplTag> is the type of a Value's `impl` field, and it
// holds one member's data (a ConstIntImpl{42}, etc.). It is not a tag or a
// wrapper around impl; it is the impl's type.
//
//   - Open    : a dialect registers a member with KVM_MARK_MEMBER(Tag, T); the
//               core never needs to name or include that member type.
//   - Bounded : only registered members may be stored; storing anything else
//               (e.g. an int) is a compile error -- the constructor simply does
//               not exist for unregistered types.
//   - Tagless : AnyOf holds no discriminator. Which member it holds is decided
//               externally (in this IR, by a Value/Operation's `type`/`op`
//               key). Recovery is `x.as<T>()`.
//
// "mark" is the registration act (KVM_MARK_MEMBER + the Tag), a compile-time
// trait carrying no data. Storage of the member's data is a separate concern:
// here it is std::any, so sizeof(AnyOf) is fixed regardless of member size, and
// empty is allowed (not every value has an impl).

#include <any>
#include <type_traits>
#include <utility>

namespace kvm::ir {

// Trait specialized by KVM_MARK_MEMBER to register T as a member of sum Tag.
template <class Tag, class T>
struct MarkMember : std::false_type {};

template <class Tag, class T>
concept InMark = MarkMember<Tag, std::remove_cvref_t<T>>::value;

template <class Tag>
class AnyOf {
 public:
  AnyOf() = default;

  // Only types registered into this sum may be stored.
  template <class T>
    requires InMark<Tag, T>
  AnyOf(T value) : data_(std::move(value)) {}

  bool has_value() const noexcept { return data_.has_value(); }

  // Recover the concrete member: a single-branch PROBE, not a unified getter.
  // `as<T>()` asks "is the stored member a T?" -- yes gives a typed pointer
  // (concrete type only inside this branch), no gives nullptr. There is
  // deliberately no `GetImpl(...) -> Some`: a sum has no "member" outside a
  // match, and a unified operation returning a unified result would demand a
  // unified interface (inheritance), not a sum. Consuming an AnyOf means
  // matching branches with as<T>(); each branch is the caller's own logic. Do
  // not add a unified extractor.
  template <class T>
    requires InMark<Tag, T>
  const T* as() const noexcept {
    return std::any_cast<T>(&data_);
  }

  template <class T>
    requires InMark<Tag, T>
  T* as() noexcept {
    return std::any_cast<T>(&data_);
  }

 private:
  std::any data_;
};

}  // namespace kvm::ir

// Register T as a member of the sum identified by Tag. Use at namespace scope.
#define KVM_MARK_MEMBER(Tag, T) \
  template <>                   \
  struct ::kvm::ir::MarkMember<Tag, T> : std::true_type {}
