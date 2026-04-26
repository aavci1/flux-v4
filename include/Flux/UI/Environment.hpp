#pragma once

/// \file Flux/UI/Environment.hpp
///
/// Part of the Flux public API.


#include <any>
#include <concepts>
#include <cstddef>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace flux {

class EnvironmentLayer {
public:
  template<typename T>
  void set(T value) {
    static_assert(std::equality_comparable<T>,
        "Environment values must define operator==. EnvironmentLayer compares values for retained-subtree reuse. "
        "For Element fields, implement operator== using Element::structuralEquals.");
    static_assert(std::is_copy_constructible_v<T>,
        "Environment values must be copy-constructible. EnvironmentLayer copies values during stack push/pop.");

    Slot& slot = values_[std::type_index(typeid(T))];
    slot.value = std::move(value);
    slot.equals = &slotEquals<T>;
  }

  template<typename T>
  T const* get() const {
    auto it = values_.find(std::type_index(typeid(T)));
    if (it == values_.end()) {
      return nullptr;
    }
    return std::any_cast<T>(&it->second.value);
  }

  bool empty() const { return values_.empty(); }
  std::size_t size() const { return values_.size(); }

  bool operator==(EnvironmentLayer const& other) const {
    if (values_.size() != other.values_.size()) {
      return false;
    }
    for (auto const& [type, slot] : values_) {
      auto it = other.values_.find(type);
      if (it == other.values_.end()) {
        return false;
      }
      if (slot.equals != it->second.equals || !slot.equals) {
        return false;
      }
      if (!slot.equals(slot.value, it->second.value)) {
        return false;
      }
    }
    return true;
  }

private:
  struct Slot {
    std::any value;
    bool (*equals)(std::any const&, std::any const&) = nullptr;
  };

  template<typename T>
  static bool slotEquals(std::any const& a, std::any const& b) {
    T const* lhs = std::any_cast<T>(&a);
    T const* rhs = std::any_cast<T>(&b);
    return lhs && rhs && *lhs == *rhs;
  }

  std::unordered_map<std::type_index, Slot> values_;
};

class EnvironmentStack {
public:
  static EnvironmentStack& current();

  void push(EnvironmentLayer layer);
  void pop();

  template<typename T>
  T const* find() const {
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
      if (T const* v = it->get<T>()) {
        return v;
      }
    }
    return nullptr;
  }

  bool empty() const { return layers_.empty(); }

private:
  std::vector<EnvironmentLayer> layers_;
};

} // namespace flux
