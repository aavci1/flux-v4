#pragma once

#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/Reactive/Signal.hpp>

#include <algorithm>
#include <cmath>

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

template<Interpolatable T>
class Animation : public AnimationBase {
public:
  Animation() = default;
  explicit Animation(T initial)
      : value_(std::move(initial))
      , start_(value_.peek())
      , target_(value_.peek()) {}

  T const& get() const { return value_.get(); }
  T const& peek() const { return value_.peek(); }
  T const& operator*() const { return value_.get(); }
  operator T() const { return value_.get(); }

  void set(T value, Transition transition = Transition::instant()) const {
    Animation* self = const_cast<Animation*>(this);
    self->running_ = false;
    self->paused_ = false;
    self->start_ = self->value_.peek();
    self->target_ = value;
    if (self->reducedMotion_ || transition.duration <= 0.f) {
      self->value_.set(std::move(value));
      return;
    }
    self->value_.set(std::move(value));
  }

  Animation const& operator=(T value) const {
    set(std::move(value), WithTransition::hasCurrent() ? WithTransition::current()
                                                       : Transition::instant());
    return *this;
  }

  void play(T target, Transition transition) const {
    play(std::move(target), AnimationOptions{.transition = std::move(transition)});
  }

  void play(T target, AnimationOptions options = {}) const {
    Animation* self = const_cast<Animation*>(this);
    self->start_ = self->value_.peek();
    self->target_ = std::move(target);
    self->options_ = std::move(options);
    self->paused_ = false;
    if (self->reducedMotion_ || self->options_.transition.duration <= 0.f) {
      self->running_ = false;
      self->value_.set(self->target_);
      return;
    }
    self->running_ = true;
    self->startTime_ = 0.0;
  }

  void pause() const {
    const_cast<Animation*>(this)->paused_ = true;
  }

  void resume() const {
    Animation* self = const_cast<Animation*>(this);
    if (self->running_) {
      self->paused_ = false;
    }
  }

  void stop() const {
    Animation* self = const_cast<Animation*>(this);
    self->running_ = false;
    self->paused_ = false;
  }

  bool isRunning() const { return running_; }
  bool isPaused() const { return paused_; }

  void setReducedMotion(bool enabled) {
    reducedMotion_ = enabled;
    if (enabled) {
      running_ = false;
      paused_ = false;
      value_.set(finalValueForOptions());
    }
  }

  ObserverHandle observe(std::function<void()> callback) const {
    return value_.observe(std::move(callback));
  }

  void unobserve(ObserverHandle handle) const {
    value_.unobserve(handle);
  }

  bool tick(double nowSeconds) override {
    if (!running_ || paused_) {
      return false;
    }
    double const duration = std::max(0.000001, static_cast<double>(options_.transition.duration));
    double elapsed = nowSeconds - startTime_ - static_cast<double>(options_.transition.delay);
    if (elapsed < 0.0) {
      value_.set(start_);
      return true;
    }

    int const repeat = options_.repeat == 0 ? 1 : options_.repeat;
    if (repeat != AnimationOptions::kRepeatForever &&
        elapsed >= duration * static_cast<double>(repeat)) {
      running_ = false;
      paused_ = false;
      value_.set(finalValueForOptions());
      return false;
    }

    int iteration = static_cast<int>(std::floor(elapsed / duration));
    double local = elapsed - static_cast<double>(iteration) * duration;
    float t = static_cast<float>(std::clamp(local / duration, 0.0, 1.0));
    if (options_.autoreverse && (iteration % 2) == 1) {
      t = 1.f - t;
    }
    if (options_.transition.easing) {
      t = std::clamp(options_.transition.easing(t), 0.f, 1.f);
    }
    value_.set(lerp(start_, target_, t));
    return true;
  }

private:
  T finalValueForOptions() const {
    int const repeat = options_.repeat == 0 ? 1 : options_.repeat;
    if (options_.autoreverse && repeat != AnimationOptions::kRepeatForever && (repeat % 2) == 0) {
      return start_;
    }
    return target_;
  }

  Reactive::Signal<T> value_{T{}};
  T start_{};
  T target_{};
  AnimationOptions options_{};
  bool running_ = false;
  bool paused_ = false;
  bool reducedMotion_ = false;
  double startTime_ = 0.0;
};

} // namespace flux
