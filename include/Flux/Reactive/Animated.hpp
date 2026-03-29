#pragma once

#include <Flux/Reactive/Detail/DependencyTracker.hpp>
#include <Flux/Reactive/Detail/Notify.hpp>
#include <Flux/Reactive/Detail/TypeTraits.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/Reactive/Transition.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

class AnimationClock;

class AnimatedBase {
public:
  virtual ~AnimatedBase() = default;
  /// Monotonic time in seconds (same domain as `TimerEvent::deadlineNanos`). Use `double` so
  /// elapsed = now - start stays accurate for sub-frame animation (float loses precision on large epochs).
  virtual bool tick(double nowSeconds) = 0;
};

namespace detail {

inline double steadyNowSeconds() {
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch());
  return static_cast<double>(ns.count()) * 1e-9;
}

} // namespace detail

template<Interpolatable T>
class Animated : public Observable, public AnimatedBase {
public:
  explicit Animated(T initial);
  ~Animated();

  Animated(Animated const&) = delete;
  Animated& operator=(Animated const&) = delete;
  Animated(Animated&&) = delete;
  Animated& operator=(Animated&&) = delete;

  T const& get() const;

  void set(T target);
  void set(T target, Transition transition);

  bool isAnimating() const { return animating_; }

  ObserverHandle observe(std::function<void()> callback) override;
  void unobserve(ObserverHandle handle) override;

private:
  friend class AnimationClock;

  bool tick(double nowSeconds) override;

  void notifyObservers();

  T current_{};
  T from_{};
  T target_{};
  double startTime_ = 0.;
  float delay_ = 0.f;
  Transition transition_{Transition::instant()};
  bool animating_ = false;

  std::uint64_t nextId_ = 1;
  std::vector<std::pair<std::uint64_t, std::function<void()>>> observers_;
};

} // namespace flux

#include <Flux/Reactive/AnimationClock.hpp>

namespace flux {

template<Interpolatable T>
Animated<T>::Animated(T initial) : current_(std::move(initial)), from_(current_), target_(current_) {}

template<Interpolatable T>
Animated<T>::~Animated() {
  AnimationClock::instance().unregisterAnimated(this);
}

template<Interpolatable T>
T const& Animated<T>::get() const {
  detail::DependencyTracker::track(const_cast<Observable*>(static_cast<Observable const*>(this)));
  return current_;
}

template<Interpolatable T>
void Animated<T>::set(T target) {
  set(std::move(target), WithTransition::current());
}

template<Interpolatable T>
void Animated<T>::set(T target, Transition transition) {
  if constexpr (detail::equalityComparableV<T>) {
    if (target_ == target) {
      return;
    }
  }
  transition_ = std::move(transition);
  target_ = std::move(target);

  if (transition_.duration <= 0.f) {
    if (animating_) {
      AnimationClock::instance().unregisterAnimated(this);
    }
    current_ = target_;
    from_ = current_;
    animating_ = false;
    notifyObservers();
    return;
  }

  from_ = current_;
  delay_ = transition_.delay;
  startTime_ = detail::steadyNowSeconds();
  animating_ = true;
  AnimationClock::instance().registerAnimated(this);
}

template<Interpolatable T>
ObserverHandle Animated<T>::observe(std::function<void()> callback) {
  const std::uint64_t id = nextId_++;
  observers_.emplace_back(id, std::move(callback));
  return ObserverHandle{id};
}

template<Interpolatable T>
void Animated<T>::unobserve(ObserverHandle handle) {
  if (!handle.isValid()) {
    return;
  }
  std::erase_if(observers_, [handle](auto const& p) { return p.first == handle.id; });
}

template<Interpolatable T>
void Animated<T>::notifyObservers() {
  detail::notifyObserverList(observers_);
}

template<Interpolatable T>
bool Animated<T>::tick(double nowSeconds) {
  if (!animating_) {
    return false;
  }

  const double elapsed = nowSeconds - startTime_ - static_cast<double>(delay_);
  if (elapsed < 0.) {
    return true;
  }

  const float dur = transition_.duration;
  float t = 1.f;
  if (dur > 0.f) {
    t = static_cast<float>(elapsed / static_cast<double>(dur));
    if (t < 0.f) {
      t = 0.f;
    }
    if (t > 1.f) {
      t = 1.f;
    }
  }

  // Animation complete — snap and exit before interpolation (progress/lerp below are for t < 1 only).
  if (t >= 1.f) {
    if constexpr (detail::equalityComparableV<T>) {
      if (current_ != target_) {
        current_ = target_;
        notifyObservers();
      }
    } else {
      current_ = target_;
      notifyObservers();
    }
    animating_ = false;
    return false;
  }

  float progress = 0.f;
  if (transition_.springFn) {
    progress = (*transition_.springFn)(t);
  } else if (transition_.easing) {
    progress = transition_.easing(t);
  }

  T const newValue = lerp(from_, target_, progress);

  if constexpr (detail::equalityComparableV<T>) {
    if (newValue != current_) {
      current_ = newValue;
      notifyObservers();
    }
  } else {
    current_ = newValue;
    notifyObservers();
  }

  return true;
}

} // namespace flux
