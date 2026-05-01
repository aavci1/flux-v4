#pragma once

#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/Reactive/Signal.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <memory>

namespace flux {

struct AnimationOptions {
  static constexpr int kRepeatForever = -1;

  Transition transition = Transition::ease();
  int repeat = 1;
  bool autoreverse = false;
};

class AnimationBase {
public:
  virtual ~AnimationBase() = default;
  virtual bool tick(double nowSeconds) = 0;
};

namespace detail {

inline double steadyNowSeconds() {
  auto const ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch());
  return static_cast<double>(ns.count()) * 1e-9;
}

} // namespace detail

template<Interpolatable T>
class Animation {
public:
  Animation()
      : Animation(T{}) {}

  explicit Animation(T initial)
      : state_(std::make_shared<State>(std::move(initial))) {}

  T const& get() const { return state().value.get(); }
  T const& evaluate() const { return get(); }
  T const& operator()() const { return get(); }
  T const& peek() const { return state().value.peek(); }
  T const& operator*() const { return get(); }
  operator T() const { return get(); }

  Reactive::Signal<T> signal() const { return state().value; }

  void set(T value, Transition transition = Transition::instant()) const {
    State& self = state();
    AnimationClock::instance().unregisterAnimation(&self);
    self.paused = false;
    self.start = self.value.peek();
    self.target = std::move(value);
    float const duration = transition.duration;
    self.options = AnimationOptions{.transition = std::move(transition)};
    if (duration <= 0.f) {
      self.running = false;
      self.value.set(self.target);
      return;
    }
    self.running = true;
    self.startTime = detail::steadyNowSeconds();
    AnimationClock::instance().registerAnimation(&self);
  }

  Animation const& operator=(T value) const {
    set(std::move(value), WithTransition::current());
    return *this;
  }

  void play(T target, Transition transition) const {
    play(std::move(target), AnimationOptions{.transition = std::move(transition)});
  }

  void play(T target, AnimationOptions options = {}) const {
    State& self = state();
    AnimationClock::instance().unregisterAnimation(&self);
    self.start = self.value.peek();
    self.target = std::move(target);
    self.options = std::move(options);
    self.paused = false;
    if (self.options.transition.duration <= 0.f) {
      self.running = false;
      self.value.set(self.target);
      return;
    }
    self.running = true;
    self.startTime = detail::steadyNowSeconds();
    AnimationClock::instance().registerAnimation(&self);
  }

  void pause() const {
    State& self = state();
    if (!self.running) {
      return;
    }
    self.paused = true;
    self.running = false;
    AnimationClock::instance().unregisterAnimation(&self);
  }

  void resume() const {
    State& self = state();
    if (!self.paused) {
      return;
    }
    self.paused = false;
    self.running = true;
    AnimationClock::instance().registerAnimation(&self);
  }

  void stop() const {
    State& self = state();
    AnimationClock::instance().unregisterAnimation(&self);
    self.running = false;
    self.paused = false;
  }

  bool isRunning() const { return state().running; }
  bool isPaused() const { return state().paused; }

private:
  struct State final : AnimationBase {
    explicit State(T initial)
        : value(std::move(initial))
        , start(value.peek())
        , target(value.peek()) {}

    ~State() override {
      AnimationClock::instance().unregisterAnimation(this);
    }

    bool tick(double nowSeconds) override {
      if (!running || paused) {
        return false;
      }
      double const duration = std::max(0.000001, static_cast<double>(options.transition.duration));
      double elapsed = nowSeconds - startTime - static_cast<double>(options.transition.delay);
      if (elapsed < 0.0) {
        value.set(start);
        return true;
      }

      int const repeat = options.repeat == 0 ? 1 : options.repeat;
      if (repeat != AnimationOptions::kRepeatForever &&
          elapsed >= duration * static_cast<double>(repeat)) {
        running = false;
        paused = false;
        value.set(finalValueForOptions());
        return false;
      }

      int iteration = static_cast<int>(std::floor(elapsed / duration));
      double local = elapsed - static_cast<double>(iteration) * duration;
      float t = static_cast<float>(std::clamp(local / duration, 0.0, 1.0));
      if (options.autoreverse && (iteration % 2) == 1) {
        t = 1.f - t;
      }
      if (options.transition.springFn) {
        t = (*options.transition.springFn)(t);
      } else if (options.transition.easing) {
        t = std::clamp(options.transition.easing(t), 0.f, 1.f);
      }
      value.set(lerp(start, target, t));
      return true;
    }

    T finalValueForOptions() const {
      int const repeat = options.repeat == 0 ? 1 : options.repeat;
      if (options.autoreverse && repeat != AnimationOptions::kRepeatForever && (repeat % 2) == 0) {
        return start;
      }
      return target;
    }

    Reactive::Signal<T> value{T{}};
    T start{};
    T target{};
    AnimationOptions options{};
    bool running = false;
    bool paused = false;
    double startTime = 0.0;
  };

  State& state() const {
    assert(state_ && "using an empty Animation handle");
    return *state_;
  }

  std::shared_ptr<State> state_;
};

} // namespace flux
