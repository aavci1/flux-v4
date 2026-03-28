#pragma once

#include <Flux/Reactive/Animated.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/StateStore.hpp>

namespace flux {

/// Returns a persistent Signal<T> for the current component instance.
/// `initial` is used only on the first call at this position.
/// Must be called in the same order every body() invocation.
template<typename T>
Signal<T>& useState(T initial = T{}) {
  StateStore* store = StateStore::current();
  assert(store && "useState called outside of a build pass");
  return store->claimSlot<Signal<T>>(std::move(initial));
}

/// Returns a persistent Animated<T> for the current component instance.
template<typename T>
Animated<T>& useAnimated(T initial = T{}) {
  StateStore* store = StateStore::current();
  assert(store && "useAnimated called outside of a build pass");
  return store->claimSlot<Animated<T>>(std::move(initial));
}

} // namespace flux
