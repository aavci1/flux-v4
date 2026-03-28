#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/Reactive/Animated.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace flux {

namespace {

constexpr auto kAnimationInterval = std::chrono::nanoseconds(1'000'000'000 / 60);

} // namespace

AnimationClock::AnimationClock() = default;

AnimationClock& AnimationClock::instance() {
  static AnimationClock sInstance;
  return sInstance;
}

void AnimationClock::install(EventQueue& q) {
  if (installed_) {
    return;
  }
  installed_ = true;
  q.on<TimerEvent>([this](TimerEvent const& e) {
    if (e.timerId != timerId_) {
      return;
    }
    onTick(e.deadlineNanos);
  });
}

void AnimationClock::shutdown() {
  stopTimer();
  active_.clear();
}

void AnimationClock::registerAnimated(AnimatedBase* animated) {
  if (!animated) {
    return;
  }
  if (std::find(active_.begin(), active_.end(), animated) != active_.end()) {
    return;
  }
  active_.push_back(animated);
  if (!running_) {
    startTimer();
  }
}

void AnimationClock::unregisterAnimated(AnimatedBase* animated) {
  if (!animated) {
    return;
  }
  std::erase(active_, animated);
  if (active_.empty()) {
    stopTimer();
  }
}

void AnimationClock::onTick(std::int64_t deadlineNanos) {
  const double now = static_cast<double>(deadlineNanos) * 1e-9;
  std::vector<AnimatedBase*> snapshot = active_;
  for (AnimatedBase* p : snapshot) {
    if (!p) {
      continue;
    }
    if (!p->tick(now)) {
      unregisterAnimated(p);
    }
  }
  if (active_.empty()) {
    stopTimer();
  }
}

void AnimationClock::startTimer() {
  if (running_) {
    return;
  }
  timerId_ = Application::instance().scheduleRepeatingTimer(kAnimationInterval, 0);
  running_ = true;
}

void AnimationClock::stopTimer() {
  if (!running_) {
    return;
  }
  if (timerId_ != 0) {
    Application::instance().cancelTimer(timerId_);
  }
  timerId_ = 0;
  running_ = false;
}

} // namespace flux
