#pragma once

/// \file Flux/Reactive/Signal.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Detail/ApplicationSignalBridge.hpp>
#include <Flux/Reactive/Detail/DependencyTracker.hpp>
#include <Flux/Reactive/Detail/Notify.hpp>
#include <Flux/Reactive/Detail/TypeTraits.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

template<typename T>
class Signal : public Observable {
public:
  explicit Signal(T initial = T{}) : value_(std::move(initial)) {}

  Signal(Signal const&) = delete;
  Signal& operator=(Signal const&) = delete;
  Signal(Signal&&) = delete;
  Signal& operator=(Signal&&) = delete;

  T const& get() const;

  void set(T value);
  void setSilently(T value);

  void notifyChanged();

  ObserverHandle observe(std::function<void()> callback) override;
  void unobserve(ObserverHandle handle) override;
  ObserverHandle observeComposite(StateStore& store, ComponentKey const& key) override;

private:
  void notifyObservers();

  T value_;
  std::uint64_t nextId_ = 1;
  std::vector<std::pair<std::uint64_t, std::function<void()>>> observers_;
  std::vector<std::pair<std::uint64_t, CompositeObserver>> compositeObservers_;
};

} // namespace flux

namespace flux {

template<typename T>
T const& Signal<T>::get() const {
  detail::DependencyTracker::track(const_cast<Observable*>(static_cast<Observable const*>(this)));
  return value_;
}

template<typename T>
void Signal<T>::set(T value) {
  if constexpr (detail::equalityComparableV<T>) {
    if (value == value_) {
      return;
    }
  }
  value_ = std::move(value);
  notifyObservers();
}

template<typename T>
void Signal<T>::setSilently(T value) {
  if constexpr (detail::equalityComparableV<T>) {
    if (value == value_) {
      return;
    }
  }
  value_ = std::move(value);
}

template<typename T>
void Signal<T>::notifyChanged() {
  notifyObservers();
}

template<typename T>
ObserverHandle Signal<T>::observe(std::function<void()> callback) {
  const std::uint64_t id = nextId_++;
  observers_.emplace_back(id, std::move(callback));
  return ObserverHandle{id};
}

template<typename T>
void Signal<T>::unobserve(ObserverHandle handle) {
  if (!handle.isValid()) {
    return;
  }
  std::erase_if(observers_, [handle](auto const& p) { return p.first == handle.id; });
  std::erase_if(compositeObservers_, [handle](auto const& p) { return p.first == handle.id; });
}

template<typename T>
ObserverHandle Signal<T>::observeComposite(StateStore& store, ComponentKey const& key) {
  std::uint64_t const id = nextId_++;
  compositeObservers_.emplace_back(id, CompositeObserver{.store = &store, .key = key});
  return ObserverHandle{id};
}

template<typename T>
void Signal<T>::notifyObservers() {
  // `useState` / internal `Signal`s can be completely observer-less; composite observers wake the
  // runtime via `StateStore::markCompositeDirty`, but a bare signal still needs to schedule the next
  // reactive rebuild here.
  if (observers_.empty() && compositeObservers_.empty() && detail::signalBridgeApplicationHasInstance()) {
    detail::signalBridgeMarkReactiveDirty();
  }
  auto compositeSnapshot = compositeObservers_;
  for (auto const& [id, observer] : compositeSnapshot) {
    (void)id;
    if (observer.store) {
      observer.store->markCompositeDirty(observer.key);
    }
  }
  detail::notifyObserverList(observers_);
}

} // namespace flux
