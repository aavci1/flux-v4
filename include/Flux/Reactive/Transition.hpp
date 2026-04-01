#pragma once

/// \file Flux/Reactive/Transition.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Easing.hpp>

#include <functional>
#include <optional>

namespace flux {

struct Transition {
  float duration = 0.25f;
  float delay = 0.f;
  EasingFn easing = Easing::easeInOut;

  std::optional<std::function<float(float)>> springFn;

  static Transition instant();
  static Transition linear(float dur);
  static Transition ease(float dur = 0.25f);
  static Transition spring(float k = 300.f, float d = 20.f, float dur = 0.6f);
};

class WithTransition {
public:
  explicit WithTransition(Transition t);
  ~WithTransition();

  WithTransition(WithTransition const&) = delete;
  WithTransition& operator=(WithTransition const&) = delete;

  static Transition current();
};

} // namespace flux
