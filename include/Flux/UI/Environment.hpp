#pragma once

/// \file Flux/UI/Environment.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Signal.hpp>

#include <any>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace flux {

template<typename T>
class EnvironmentValue {
public:
  EnvironmentValue(T const& value)
      : value_(&value) {}

  EnvironmentValue(Reactive::Signal<T> signal)
      : signal_(std::move(signal))
      , reactive_(true) {}

  T const& get() const {
    return reactive_ ? signal_.get() : *value_;
  }

  T const& peek() const {
    return reactive_ ? signal_.peek() : *value_;
  }

  T const& operator()() const {
    return get();
  }

  T const& operator*() const {
    return get();
  }

  operator T const&() const {
    return get();
  }

  void set(T value) const {
    assert(reactive_ && "environment value is not backed by a Signal");
    signal_.set(std::move(value));
  }

  bool reactive() const noexcept {
    return reactive_;
  }

  Reactive::Signal<T> const* signal() const noexcept {
    return reactive_ ? &signal_ : nullptr;
  }

private:
  T const* value_ = nullptr;
  Reactive::Signal<T> signal_{};
  bool reactive_ = false;
};

class EnvironmentLayer {
public:
  template<typename T>
  void set(T value) {
    static_assert(std::equality_comparable<T>,
        "Environment values must define operator==.");
    static_assert(std::is_copy_constructible_v<T>,
        "Environment values must be copy-constructible. EnvironmentLayer copies values during stack push/pop.");

    Slot& slot = values_[std::type_index(typeid(T))];
    slot.value = std::move(value);
    slot.equals = &slotEquals<T>;
  }

  template<typename T>
  void setSignal(Reactive::Signal<T> signal) {
    static_assert(std::equality_comparable<T>,
        "Environment signal values must define operator==.");

    Slot& slot = values_[std::type_index(typeid(T))];
    slot.value = std::move(signal);
    slot.equals = &signalSlotEquals<T>;
  }

  template<typename T>
  T const* get() const {
    auto it = values_.find(std::type_index(typeid(T)));
    if (it == values_.end()) {
      return nullptr;
    }
    if (T const* value = std::any_cast<T>(&it->second.value)) {
      return value;
    }
    if (auto const* signal = std::any_cast<Reactive::Signal<T>>(&it->second.value)) {
      return &signal->peek();
    }
    return nullptr;
  }

  template<typename T>
  Reactive::Signal<T> const* signal() const {
    auto it = values_.find(std::type_index(typeid(T)));
    if (it == values_.end()) {
      return nullptr;
    }
    return std::any_cast<Reactive::Signal<T>>(&it->second.value);
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

  template<typename T>
  static bool signalSlotEquals(std::any const& a, std::any const& b) {
    auto const* lhs = std::any_cast<Reactive::Signal<T>>(&a);
    auto const* rhs = std::any_cast<Reactive::Signal<T>>(&b);
    return lhs && rhs && lhs->peek() == rhs->peek();
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

  template<typename T>
  Reactive::Signal<T> const* findSignal() const {
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
      if (auto const* signal = it->signal<T>()) {
        return signal;
      }
    }
    return nullptr;
  }

  bool empty() const { return layers_.empty(); }
  std::vector<EnvironmentLayer> snapshot() const { return layers_; }

private:
  std::vector<EnvironmentLayer> layers_;
};

} // namespace flux
