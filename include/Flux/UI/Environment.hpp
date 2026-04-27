#pragma once

/// \file Flux/UI/Environment.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/Detail/EnvironmentSlot.hpp>

#include <any>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

template<typename Tag>
struct EnvironmentKey;

#define FLUX_DEFINE_ENVIRONMENT_KEY(KeyTag, ValueT, DefaultExpr)                         \
  struct KeyTag {};                                                                       \
  template<>                                                                              \
  struct EnvironmentKey<KeyTag> {                                                         \
    using Value = ValueT;                                                                 \
    static_assert(std::copy_constructible<Value>,                                         \
                  "Environment key values must be copy-constructible.");                  \
    static_assert(std::equality_comparable<Value>,                                        \
                  "Environment key values must define operator==.");                      \
    static Value defaultValue() { return (DefaultExpr); }                                 \
    static ::flux::detail::EnvironmentSlot const& slot() {                                \
      static ::flux::detail::EnvironmentSlot s{                                           \
          ::flux::detail::allocateEnvironmentSlot(typeid(KeyTag))};                       \
      return s;                                                                           \
    }                                                                                     \
  }

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

    Slot& slot = upsert(std::type_index(typeid(T)));
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
    Slot& slot = upsert(std::type_index(typeid(T)));
    slot.value = std::move(signal);
    slot.equals = &signalSlotEquals<T>;
  }

  template<typename T>
  T const* get() const {
    Slot const* slot = find(std::type_index(typeid(T)));
    if (!slot) {
      return nullptr;
    }
    if (T const* value = std::any_cast<T>(&slot->value)) {
      return value;
    }
    if (auto const* signal = std::any_cast<Reactive::Signal<T>>(&slot->value)) {
      return &signal->peek();
    }
    return nullptr;
  }

  template<typename T>
  Reactive::Signal<T> const* signal() const {
    Slot const* slot = find(std::type_index(typeid(T)));
    if (!slot) {
      return nullptr;
    }
    return std::any_cast<Reactive::Signal<T>>(&slot->value);
  }

  bool empty() const { return entries_.empty(); }
  std::size_t size() const { return entries_.size(); }

  bool operator==(EnvironmentLayer const& other) const {
    if (entries_.size() != other.entries_.size()) {
      return false;
    }
    for (Entry const& entry : entries_) {
      Slot const* otherSlot = other.find(entry.type);
      if (!otherSlot) {
        return false;
      }
      Slot const& slot = entry.slot;
      if (slot.equals != otherSlot->equals || !slot.equals) {
        return false;
      }
      if (!slot.equals(slot.value, otherSlot->value)) {
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

  struct Entry {
    std::type_index type;
    Slot slot;
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

  Slot* find(std::type_index type) {
    for (Entry& entry : entries_) {
      if (entry.type == type) {
        return &entry.slot;
      }
    }
    return nullptr;
  }

  Slot const* find(std::type_index type) const {
    for (Entry const& entry : entries_) {
      if (entry.type == type) {
        return &entry.slot;
      }
    }
    return nullptr;
  }

  Slot& upsert(std::type_index type) {
    if (Slot* slot = find(type)) {
      return *slot;
    }
    entries_.push_back(Entry{type, Slot{}});
    return entries_.back().slot;
  }

  std::vector<Entry> entries_;
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
