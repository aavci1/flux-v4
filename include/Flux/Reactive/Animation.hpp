#pragma once

/// \file Flux/Reactive/Animation.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Detail/DependencyTracker.hpp>
#include <Flux/Reactive/Detail/AnimationRebuild.hpp>
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

class AnimationBase {
public:
  virtual ~AnimationBase() = default;
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

struct AnimationOptions {
  static constexpr int kRepeatForever = -1;

  Transition transition = Transition::ease();
  int repeat = 1;
  bool autoreverse = false;
};

template<Interpolatable T>
class Animation : public Observable, public AnimationBase {
public:
  explicit Animation(T initial);
  Animation(T initial, AnimationOptions options);
  ~Animation();

  Animation(Animation const&) = delete;
  Animation& operator=(Animation const&) = delete;
  Animation(Animation&&) = delete;
  Animation& operator=(Animation&&) = delete;

  T const& get() const;

  void set(T target);
  void set(T target, Transition transition);
  void set(T target, AnimationOptions options);
  void play(T target);
  void play(T target, Transition transition);
  void play(T target, AnimationOptions options);
  void pause();
  void resume();
  void stop();
  void setReducedMotion(bool reducedMotion);
  void setOptions(AnimationOptions options);

  bool isAnimating() const { return animating_; }
  bool isPaused() const { return paused_; }
  bool isRunning() const { return animating_ || paused_; }

  ObserverHandle observe(std::function<void()> callback) override;
  void unobserve(ObserverHandle handle) override;

private:
  friend class AnimationClock;

  bool tick(double nowSeconds) override;

  static AnimationOptions instantOptions();
  AnimationOptions normalizedOptions(AnimationOptions options) const;
  AnimationOptions resolvedSetOptions() const;
  void startPlayback(T target, AnimationOptions options);
  void notifyObservers();

  T current_{};
  T from_{};
  T target_{};
  double startTime_ = 0.;
  double pausedAt_ = 0.;
  float delay_ = 0.f;
  AnimationOptions options_ = instantOptions();
  AnimationOptions activeOptions_ = instantOptions();
  bool animating_ = false;
  bool paused_ = false;
  bool reducedMotion_ = false;

  std::uint64_t nextId_ = 1;
  std::vector<std::pair<std::uint64_t, std::function<void()>>> observers_;
};

} // namespace flux

#include <Flux/Reactive/AnimationClock.hpp>

namespace flux {

template<Interpolatable T>
AnimationOptions Animation<T>::instantOptions() {
  AnimationOptions options{};
  options.transition = Transition::instant();
  return options;
}

template<Interpolatable T>
Animation<T>::Animation(T initial) : Animation(std::move(initial), instantOptions()) {}

template<Interpolatable T>
Animation<T>::Animation(T initial, AnimationOptions options)
    : current_(std::move(initial)), from_(current_), target_(current_), options_(std::move(options)),
      activeOptions_(options_) {}

template<Interpolatable T>
Animation<T>::~Animation() {
  AnimationClock::instance().unregisterAnimation(this);
}

template<Interpolatable T>
T const& Animation<T>::get() const {
  detail::DependencyTracker::track(const_cast<Observable*>(static_cast<Observable const*>(this)));
  return current_;
}

template<Interpolatable T>
void Animation<T>::set(T target) {
  set(std::move(target), resolvedSetOptions());
}

template<Interpolatable T>
void Animation<T>::set(T target, Transition transition) {
  AnimationOptions options = options_;
  options.transition = std::move(transition);
  set(std::move(target), std::move(options));
}

template<Interpolatable T>
void Animation<T>::set(T target, AnimationOptions options) {
  if constexpr (detail::equalityComparableV<T>) {
    if (target_ == target) {
      return;
    }
  }
  play(std::move(target), std::move(options));
}

template<Interpolatable T>
void Animation<T>::play(T target) {
  startPlayback(std::move(target), resolvedSetOptions());
}

template<Interpolatable T>
void Animation<T>::play(T target, Transition transition) {
  AnimationOptions options = options_;
  options.transition = std::move(transition);
  play(std::move(target), std::move(options));
}

template<Interpolatable T>
void Animation<T>::play(T target, AnimationOptions options) {
  startPlayback(std::move(target), std::move(options));
}

template<Interpolatable T>
void Animation<T>::pause() {
  if (!animating_) {
    return;
  }
  pausedAt_ = detail::steadyNowSeconds();
  animating_ = false;
  paused_ = true;
  AnimationClock::instance().unregisterAnimation(this);
}

template<Interpolatable T>
void Animation<T>::resume() {
  if (!paused_) {
    return;
  }
  double const now = detail::steadyNowSeconds();
  startTime_ += now - pausedAt_;
  pausedAt_ = 0.;
  paused_ = false;
  animating_ = true;
  AnimationClock::instance().registerAnimation(this);
}

template<Interpolatable T>
void Animation<T>::stop() {
  if (animating_) {
    AnimationClock::instance().unregisterAnimation(this);
  }
  animating_ = false;
  paused_ = false;
  pausedAt_ = 0.;
  from_ = current_;
  target_ = current_;
}

template<Interpolatable T>
void Animation<T>::setReducedMotion(bool reducedMotion) {
  if (reducedMotion_ == reducedMotion) {
    return;
  }
  reducedMotion_ = reducedMotion;
  if (!reducedMotion_ || (!animating_ && !paused_)) {
    return;
  }
  if (animating_) {
    AnimationClock::instance().unregisterAnimation(this);
  }
  animating_ = false;
  paused_ = false;
  pausedAt_ = 0.;
  if constexpr (detail::equalityComparableV<T>) {
    if (current_ == target_) {
      from_ = current_;
      return;
    }
  }
  current_ = target_;
  from_ = current_;
  notifyObservers();
}

template<Interpolatable T>
void Animation<T>::setOptions(AnimationOptions options) {
  options_ = normalizedOptions(std::move(options));
}

template<Interpolatable T>
AnimationOptions Animation<T>::normalizedOptions(AnimationOptions options) const {
  if (options.repeat != AnimationOptions::kRepeatForever && options.repeat < 1) {
    options.repeat = 1;
  }
  if (!reducedMotion_) {
    return options;
  }
  options.transition = Transition::instant();
  return options;
}

template<Interpolatable T>
AnimationOptions Animation<T>::resolvedSetOptions() const {
  AnimationOptions options = options_;
  if (WithTransition::hasCurrent()) {
    options.transition = WithTransition::current();
  }
  return options;
}

template<Interpolatable T>
void Animation<T>::startPlayback(T target, AnimationOptions options) {
  activeOptions_ = normalizedOptions(std::move(options));
  target_ = std::move(target);
  paused_ = false;
  pausedAt_ = 0.;

  if (activeOptions_.transition.duration <= 0.f) {
    if (animating_) {
      AnimationClock::instance().unregisterAnimation(this);
    }
    current_ = target_;
    from_ = current_;
    animating_ = false;
    notifyObservers();
    return;
  }

  from_ = current_;
  delay_ = activeOptions_.transition.delay;
  startTime_ = detail::steadyNowSeconds();
  animating_ = true;
  AnimationClock::instance().registerAnimation(this);
}

template<Interpolatable T>
ObserverHandle Animation<T>::observe(std::function<void()> callback) {
  const std::uint64_t id = nextId_++;
  observers_.emplace_back(id, std::move(callback));
  return ObserverHandle{id};
}

template<Interpolatable T>
void Animation<T>::unobserve(ObserverHandle handle) {
  if (!handle.isValid()) {
    return;
  }
  std::erase_if(observers_, [handle](auto const& p) { return p.first == handle.id; });
}

template<Interpolatable T>
void Animation<T>::notifyObservers() {
  // `detail::notifyObserverList` only calls `markReactiveDirty` when explicit `observe()` callbacks
  // exist. Views that read `Animation` in `body()` (without a Computed dependency tracker) have no
  // callbacks, so animation ticks must still schedule a rebuild to refresh scene nodes.
  detail::scheduleReactiveRebuildAfterAnimationChange();
  detail::notifyObserverList(observers_);
}

template<Interpolatable T>
bool Animation<T>::tick(double nowSeconds) {
  if (!animating_) {
    return false;
  }

  const double elapsed = nowSeconds - startTime_ - static_cast<double>(delay_);
  if (elapsed < 0.) {
    return true;
  }

  const float dur = activeOptions_.transition.duration;
  double const rawProgress = dur > 0.f ? elapsed / static_cast<double>(dur) : 1.;
  bool const repeatsForever = activeOptions_.repeat == AnimationOptions::kRepeatForever;
  int const iterationCount = activeOptions_.repeat < 1 ? 1 : activeOptions_.repeat;

  // Animation complete — snap to the correct terminal endpoint before interpolation.
  if (!repeatsForever && rawProgress >= static_cast<double>(iterationCount)) {
    T const finalValue = (activeOptions_.autoreverse && (iterationCount % 2 == 0)) ? from_ : target_;
    if constexpr (detail::equalityComparableV<T>) {
      if (current_ != finalValue) {
        current_ = finalValue;
        notifyObservers();
      }
    } else {
      current_ = finalValue;
      notifyObservers();
    }
    animating_ = false;
    return false;
  }

  double cycleProgress = rawProgress;
  if (cycleProgress < 0.) {
    cycleProgress = 0.;
  }
  int const iterationIndex = static_cast<int>(std::floor(cycleProgress));
  float t = static_cast<float>(cycleProgress - static_cast<double>(iterationIndex));
  if (t < 0.f) {
    t = 0.f;
  }
  if (t > 1.f) {
    t = 1.f;
  }

  float progress = 0.f;
  if (activeOptions_.transition.springFn) {
    progress = (*activeOptions_.transition.springFn)(t);
  } else if (activeOptions_.transition.easing) {
    progress = activeOptions_.transition.easing(t);
  }

  bool const forward = !activeOptions_.autoreverse || ((iterationIndex % 2) == 0);
  T const& cycleFrom = forward ? from_ : target_;
  T const& cycleTarget = forward ? target_ : from_;
  T const newValue = lerp(cycleFrom, cycleTarget, progress);

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
