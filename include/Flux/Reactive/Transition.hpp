#pragma once

#include <Flux/Reactive/Easing.hpp>

#include <functional>
#include <optional>

namespace flux {

struct Transition {
  float duration = 0.25f;
  float delay = 0.f;
  EasingFn easing = Easing::easeInOut;

  std::optional<std::function<float(float)>> springFn;

  static Transition instant() {
    Transition t{};
    t.duration = 0.f;
    return t;
  }
  static Transition linear(float dur) {
    Transition t{};
    t.duration = dur;
    t.easing = Easing::linear;
    return t;
  }
  static Transition ease(float dur = 0.25f) {
    Transition t{};
    t.duration = dur;
    t.easing = Easing::easeInOut;
    return t;
  }
  static Transition spring(float k = 300.f, float d = 20.f, float dur = 0.6f) {
    Transition t{};
    t.duration = dur;
    t.delay = 0.f;
    t.easing = Easing::linear;
    t.springFn = Easing::spring(k, d);
    return t;
  }
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
