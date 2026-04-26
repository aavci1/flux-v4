#pragma once

/// \file Flux/Reactive/Observer.hpp
///
/// Part of the Flux public API.


#include <cstdint>
#include <functional>

namespace flux {

/// Opaque handle returned by `Observable::observe`. Pass to `Observable::unobserve`.
struct ObserverHandle {
  std::uint64_t id = 0;
  bool isValid() const;
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
};

} // namespace flux
