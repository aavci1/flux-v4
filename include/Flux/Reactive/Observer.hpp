#pragma once

/// \file Flux/Reactive/Observer.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/ComponentKey.hpp>

#include <cstdint>
#include <functional>

namespace flux {

class StateStore;

/// Opaque handle returned by `Observable::observe`. Pass to `Observable::unobserve`.
struct ObserverHandle {
  std::uint64_t id = 0;
  bool isValid() const;
};

struct CompositeObserver {
  StateStore* store = nullptr;
  ComponentKey key{};
};

/// Interface implemented by Signal<T>, Computed<T>, and Animation<T>.
class Observable {
public:
  virtual ~Observable() = default;

  /// Register a callback fired whenever the value changes. Returns a handle for removal.
  /// The callback receives no arguments — callers call get() themselves.
  /// All callbacks fire on the main thread.
  virtual ObserverHandle observe(std::function<void()> callback) = 0;

  /// Remove a previously registered observer. No-op for invalid handles.
  virtual void unobserve(ObserverHandle handle) = 0;

  /// Register a component-key observer used by incremental rebuild.
  virtual ObserverHandle observeComposite(StateStore& store, ComponentKey const& key) = 0;
};

} // namespace flux
