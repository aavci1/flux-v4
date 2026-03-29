#pragma once

#include <Flux/Reactive/Detail/ApplicationSignalBridge.hpp>
#include <Flux/Reactive/Detail/DependencyTracker.hpp>
#include <Flux/Reactive/Detail/Notify.hpp>
#include <Flux/Reactive/Detail/TypeTraits.hpp>
#include <Flux/Reactive/Observer.hpp>

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

  void notifyChanged();

  ObserverHandle observe(std::function<void()> callback) override;
  void unobserve(ObserverHandle handle) override;

private:
  void notifyObservers();

  T value_;
  std::uint64_t nextId_ = 1;
  std::vector<std::pair<std::uint64_t, std::function<void()>>> observers_;
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
}

template<typename T>
void Signal<T>::notifyObservers() {
  // `useState` / internal `Signal`s often have no `observe()` callbacks; `notifyObserverList` would
  // otherwise skip `markReactiveDirty`, so `Runtime`'s next-frame rebuild never runs (resize still
  // rebuilt via `WindowEvent::Resize`).
  if (observers_.empty() && detail::signalBridgeApplicationHasInstance()) {
    detail::signalBridgeMarkReactiveDirty();
  }
  detail::notifyObserverList(observers_);
}

} // namespace flux
