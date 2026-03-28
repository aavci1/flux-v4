#pragma once

#include <Flux/Reactive/Animated.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/StateStore.hpp>

#include <utility>

namespace flux {

/// Copyable handle to the persistent `Signal<T>` for this hook slot. Capturing a
/// `State<T>` in a lambda copies the pointer — safe for the window lifetime.
template<typename T>
struct State {
  Signal<T>* signal = nullptr;

  explicit State(Signal<T>* s) : signal(s) {}

  T const& operator*() const { return signal->get(); }
  operator T const&() const { return signal->get(); }
  /// Mutates through the stored pointer — safe to call on a const `State` (e.g. in a lambda capture).
  void operator=(T value) const { signal->set(std::move(value)); }
};

/// Copyable handle to the persistent `Animated<T>` for this hook slot.
template<Interpolatable T>
struct Anim {
  Animated<T>* animated = nullptr;

  explicit Anim(Animated<T>* a) : animated(a) {}

  T const& operator*() const { return animated->get(); }
  operator T const&() const { return animated->get(); }
  /// Uses `WithTransition::current()` when set inside a `WithTransition` scope.
  void operator=(T value) const { animated->set(std::move(value)); }
};

/// Returns a persistent `Signal<T>` for the current component instance.
/// `initial` is used only on the first call at this position.
/// Must be called in the same order every body() invocation.
template<typename T>
State<T> useState(T initial = T{}) {
  StateStore* store = StateStore::current();
  assert(store && "useState called outside of a build pass");
  return State<T>{&store->claimSlot<Signal<T>>(std::move(initial))};
}

/// Returns a persistent `Animated<T>` for the current component instance.
template<Interpolatable T>
Anim<T> useAnimated(T initial = T{}) {
  StateStore* store = StateStore::current();
  assert(store && "useAnimated called outside of a build pass");
  return Anim<T>{&store->claimSlot<Animated<T>>(std::move(initial))};
}

} // namespace flux
