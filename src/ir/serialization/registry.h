#pragma once

// Codec registry for the textual format.
//
// The impls own their own text (T::Serialize() / T::Deserialize(sv)); they know
// only about THEMSELVES, never about AnyOf or the open sum. This registry is
// the framework side: it maps a runtime key "dialect.name" to a small codec
// that knows the concrete T, so it can pick the right T::Serialize branch when
// writing and wrap T::Deserialize back into AnyOf<...> when reading.
//
// This is exactly the open-sum dispatch we settled on: there is no unified
// GetImpl; instead a registered codec per member carries the one branch (the
// as<T>() / T construction) for that member. Registering a member is one line
// and never touches the core.

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "value.h"

namespace kvm::ir::serial {

// Key for a leaf impl: its owning type/op identity "dialect.name".
inline std::string KeyOf(const Type& t) {
  return t.dialect.empty() ? t.name : t.dialect + "." + t.name;
}
inline std::string KeyOf(const Operator& op) {
  return op.dialect.empty() ? op.name : op.dialect + "." + op.name;
}

class Registry {
 public:
  // --- registration (one line per impl member) ---

  template <class T>
  void RegisterValueImpl(std::string key) {
    value_ser_[key] = [](const AnyOf<ValueImpl>& a) {
      return a.template as<T>()->Serialize();
    };
    value_de_[std::move(key)] = [](std::string_view s) {
      return AnyOf<ValueImpl>(T::Deserialize(s));
    };
  }

  template <class T>
  void RegisterOperationImpl(std::string key) {
    op_ser_[key] = [](const AnyOf<OperationImpl>& a) {
      return a.template as<T>()->Serialize();
    };
    op_de_[std::move(key)] = [](std::string_view s) {
      return AnyOf<OperationImpl>(T::Deserialize(s));
    };
  }

  // Operators are not reconstructed from text -- the serialized form only
  // carries "dialect.name". A dialect registers each operator's full definition
  // (with its input/output type signature) here, and the parser looks it up by
  // key. (Types are just (name, dialect), so they need no registration: the
  // parser builds them directly from the two strings.)
  void RegisterOperator(const std::string& key, Operator op) {
    operators_[key] = std::move(op);
  }
  std::optional<Operator> GetOperator(const std::string& key) const {
    auto it = operators_.find(key);
    if (it == operators_.end()) return std::nullopt;
    return it->second;
  }

  // --- serialize: returns the text inside `<...>`, or nullopt if no codec
  // (e.g. an impl with no registered leaf codec, such as the structural
  // BlockOpImpl, which the serializer handles itself). ---

  std::optional<std::string> SerializeValueImpl(
      const std::string& key, const AnyOf<ValueImpl>& a) const {
    auto it = value_ser_.find(key);
    if (it == value_ser_.end()) return std::nullopt;
    return it->second(a);
  }
  std::optional<std::string> SerializeOperationImpl(
      const std::string& key, const AnyOf<OperationImpl>& a) const {
    auto it = op_ser_.find(key);
    if (it == op_ser_.end()) return std::nullopt;
    return it->second(a);
  }

  // --- deserialize: build the impl from its `<...>` text. ---

  std::optional<AnyOf<ValueImpl>> DeserializeValueImpl(
      const std::string& key, std::string_view text) const {
    auto it = value_de_.find(key);
    if (it == value_de_.end()) return std::nullopt;
    return it->second(text);
  }
  std::optional<AnyOf<OperationImpl>> DeserializeOperationImpl(
      const std::string& key, std::string_view text) const {
    auto it = op_de_.find(key);
    if (it == op_de_.end()) return std::nullopt;
    return it->second(text);
  }

 private:
  using VSer = std::function<std::string(const AnyOf<ValueImpl>&)>;
  using VDe = std::function<AnyOf<ValueImpl>(std::string_view)>;
  using OSer = std::function<std::string(const AnyOf<OperationImpl>&)>;
  using ODe = std::function<AnyOf<OperationImpl>(std::string_view)>;

  std::unordered_map<std::string, VSer> value_ser_;
  std::unordered_map<std::string, VDe> value_de_;
  std::unordered_map<std::string, OSer> op_ser_;
  std::unordered_map<std::string, ODe> op_de_;
  std::unordered_map<std::string, Operator> operators_;
};

}  // namespace kvm::ir::serial
