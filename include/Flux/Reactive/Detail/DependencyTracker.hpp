#pragma once

/// \file Flux/Reactive/Detail/DependencyTracker.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Observer.hpp>

#include <vector>

namespace flux::detail {

struct DependencyTracker {
  std::vector<Observable*> deps;

  static DependencyTracker* current();
  static void push(DependencyTracker* tracker);
  static void pop();
  static void track(Observable* o);
};

} // namespace flux::detail
