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

namespace detail {

inline std::vector<bool*>& environmentReadTrackingStack() {
  static thread_local std::vector<bool*> stack;
  return stack;
}

inline void noteEnvironmentRead() {
  for (bool* observed : environmentReadTrackingStack()) {
    if (observed) {
      *observed = true;
    }
  }
}

class EnvironmentReadTrackingScope {
public:
  EnvironmentReadTrackingScope() {
    environmentReadTrackingStack().push_back(&observed_);
  }

  EnvironmentReadTrackingScope(EnvironmentReadTrackingScope const&) = delete;
  EnvironmentReadTrackingScope& operator=(EnvironmentReadTrackingScope const&) = delete;

  ~EnvironmentReadTrackingScope() {
    auto& stack = environmentReadTrackingStack();
    if (!stack.empty()) {
      stack.pop_back();
    }
  }

  bool observed() const noexcept {
    return observed_;
  }

private:
  bool observed_ = false;
};

} // namespace detail

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

#ifndef NDEBUG
    signal.setUntrackedReadWarning(
        "useEnvironment value read outside tracking context - read inside a Bindable closure or Effect body for reactive updates.");
#endif
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
private:
  struct Entry {
    EnvironmentLayer owned{};
    EnvironmentLayer const* borrowed = nullptr;

    EnvironmentLayer const& layer() const noexcept {
      return borrowed ? *borrowed : owned;
    }
  };

public:
  static EnvironmentStack& current();

  void push(EnvironmentLayer layer);
  void pushBorrowed(EnvironmentLayer const& layer);
  void pop();

  template<typename T>
  T const* find() const {
    detail::noteEnvironmentRead();
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
      if (T const* v = it->layer().get<T>()) {
        return v;
      }
    }
    return nullptr;
  }

  template<typename T>
  Reactive::Signal<T> const* findSignal() const {
    detail::noteEnvironmentRead();
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
      if (auto const* signal = it->layer().signal<T>()) {
        return signal;
      }
    }
    return nullptr;
  }

  bool empty() const { return layers_.empty(); }
  std::vector<EnvironmentLayer> snapshot() const {
    std::vector<EnvironmentLayer> out;
    out.reserve(layers_.size());
    for (Entry const& entry : layers_) {
      out.push_back(entry.layer());
    }
    return out;
  }

private:
  std::vector<Entry> layers_;
};

} // namespace flux
