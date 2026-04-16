#pragma once

/// \file Flux/Reactive/Computed.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Detail/DependencyTracker.hpp>
#include <Flux/Reactive/Detail/Notify.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace flux {

template<typename T>
class Computed : public Observable {
public:
  explicit Computed(std::function<T()> f);
  ~Computed();

  T const& get() const;

  ObserverHandle observe(std::function<void()> callback) override;
  void unobserve(ObserverHandle handle) override;
  ObserverHandle observeComposite(StateStore& store, ComponentKey const& key) override;

private:
  void recompute() const;
  void markDirty() const;

  std::function<T()> fn_;
  mutable T value_;
  mutable bool dirty_ = true;

  mutable std::vector<std::pair<Observable*, ObserverHandle>> depHandles_;

  std::uint64_t nextId_ = 1;
  mutable std::vector<std::pair<std::uint64_t, std::function<void()>>> observers_;
  mutable std::vector<std::pair<std::uint64_t, CompositeObserver>> compositeObservers_;
};

} // namespace flux

namespace flux {

template<typename T>
Computed<T>::Computed(std::function<T()> f) : fn_(std::move(f)) {
  recompute();
}

template<typename T>
Computed<T>::~Computed() {
  for (auto& dh : depHandles_) {
    if (dh.first) {
      dh.first->unobserve(dh.second);
    }
  }
}

template<typename T>
T const& Computed<T>::get() const {
  if (dirty_) {
    recompute();
  }
  detail::DependencyTracker::track(const_cast<Observable*>(static_cast<Observable const*>(this)));
  return value_;
}

template<typename T>
ObserverHandle Computed<T>::observe(std::function<void()> callback) {
  const std::uint64_t id = nextId_++;
  observers_.emplace_back(id, std::move(callback));
  return ObserverHandle{id};
}

template<typename T>
void Computed<T>::unobserve(ObserverHandle handle) {
  if (!handle.isValid()) {
    return;
  }
  std::erase_if(observers_, [handle](auto const& p) { return p.first == handle.id; });
  std::erase_if(compositeObservers_, [handle](auto const& p) { return p.first == handle.id; });
}

template<typename T>
ObserverHandle Computed<T>::observeComposite(StateStore& store, ComponentKey const& key) {
  std::uint64_t const id = nextId_++;
  compositeObservers_.emplace_back(id, CompositeObserver{.store = &store, .key = key});
  return ObserverHandle{id};
}

template<typename T>
void Computed<T>::recompute() const {
  for (auto& dh : depHandles_) {
    if (dh.first) {
      dh.first->unobserve(dh.second);
    }
  }
  depHandles_.clear();

  detail::DependencyTracker tracker;
  detail::DependencyTracker::push(&tracker);
  T next;
  try {
    next = fn_();
  } catch (...) {
    detail::DependencyTracker::pop();
    throw;
  }
  detail::DependencyTracker::pop();

  value_ = std::move(next);
  dirty_ = false;

  {
    auto& deps = tracker.deps;
    std::sort(deps.begin(), deps.end());
    deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
  }

  Computed const* self = this;
  for (Observable* dep : tracker.deps) {
    if (!dep) {
      continue;
    }
    ObserverHandle h = dep->observe([self]() { self->markDirty(); });
    depHandles_.emplace_back(dep, h);
  }
}

template<typename T>
void Computed<T>::markDirty() const {
  dirty_ = true;
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
