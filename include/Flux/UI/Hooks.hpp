#pragma once

#include <Flux/Reactive/Animated.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace flux {

namespace detail {

inline std::size_t combineHash(std::size_t seed, std::size_t h) {
  return seed ^ (h + 0x9e3779b9u + (seed << 6) + (seed >> 2));
}

inline std::size_t hashDeps() { return 0; }

template<typename First, typename... Rest>
inline std::size_t hashDeps(First const& first, Rest const&... rest) {
  std::size_t const h = std::hash<std::decay_t<First>>{}(first);
  return combineHash(h, hashDeps(rest...));
}

} // namespace detail

/// Cached memo slot for `useMemo` — holds the last value and dependency hash.
template<typename T>
struct MemoSlot {
  T value{};
  std::size_t depsHash = 0;
  bool initialised = false;
};

/// Copyable handle to the persistent `Signal<T>` for this hook slot. Capturing a
/// `State<T>` in a lambda copies the pointer — safe for the window lifetime.
template<typename T>
struct State {
  Signal<T>* signal = nullptr;

  /// Default-constructed `State` is invalid (`signal == nullptr`). Only use default
  /// construction when the handle will be assigned before `operator*` or conversion.
  State() = default;
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

/// Returns true if the calling component's subtree contains the window's focused node.
/// Must be called inside body() like other hooks.
bool useFocus();

/// Returns a cached value of `fn()`, recomputing only when the combined hash of `deps...`
/// changes from the previous call.
///
/// This is **not** reactive: it does not subscribe to signals. It only avoids redundant work
/// within and across rebuilds when dependencies are unchanged.
///
/// Must be called in the same order every `body()` invocation (same rules as `useState`).
/// Each `deps` value is hashed with `std::hash` and mixed into a single fingerprint; types
/// without `std::hash` fail to compile at the call site.
///
/// The returned reference is valid for the duration of the current `body()` call only. Copy
/// values into lambdas that may outlive `body()`.
///
/// If `T` holds pointers or `std::string_view` into storage not owned by the memo slot, you
/// must ensure that storage outlives the slot (typically the window lifetime). Prefer returning
/// owning values from `fn()`.
///
/// With **zero dependencies**, the hash is always `0` and `fn` runs once (first initialisation)
/// then never again — useful for one-time setup, easy to misuse if you omit a dependency.
///
/// `fn` should return a value type; the implementation stores `std::decay_t` of the result.
template<typename Fn, typename... Deps>
std::decay_t<std::invoke_result_t<Fn&&>> const& useMemo(Fn&& fn, Deps const&... deps) {
  using T = std::decay_t<std::invoke_result_t<Fn&&>>;
  StateStore* store = StateStore::current();
  assert(store && "useMemo called outside of a build pass");

  MemoSlot<T>& slot = store->claimSlot<MemoSlot<T>>();

  std::size_t const currentHash = detail::hashDeps(deps...);
  if (!slot.initialised || slot.depsHash != currentHash) {
    slot.value = std::invoke(std::forward<Fn>(fn));
    slot.depsHash = currentHash;
    slot.initialised = true;
  }
  return slot.value;
}

} // namespace flux
