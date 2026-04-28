#pragma once

/// \file Flux/Reactive/Observer.hpp
///
/// Part of the Flux public API.


#include <cstdint>

namespace flux {

/// Opaque handle returned by subscription-style APIs such as AnimationClock.
struct ObserverHandle {
  std::uint64_t id = 0;
  bool isValid() const;
};

} // namespace flux
