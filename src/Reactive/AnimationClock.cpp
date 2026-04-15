#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/Reactive/Animation.hpp>

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
  if (needsTimer() && !running_) {
    startTimer();
  }
}

void AnimationClock::shutdown() {
  stopTimer();
  active_.clear();
  subscribers_.clear();
  nextSubscriberId_ = 1;
}

bool AnimationClock::needsTimer() const {
  return !active_.empty() || !subscribers_.empty();
}

void AnimationClock::registerAnimation(AnimationBase* animation) {
  if (!animation) {
    return;
  }
  if (std::find(active_.begin(), active_.end(), animation) != active_.end()) {
    return;
  }
  active_.push_back(animation);
  if (!running_) {
    startTimer();
  }
}

void AnimationClock::unregisterAnimation(AnimationBase* animation) {
  if (!animation) {
    return;
  }
  std::erase(active_, animation);
  if (!needsTimer()) {
    stopTimer();
  }
}

ObserverHandle AnimationClock::subscribe(std::function<void(AnimationTick const&)> callback) {
  if (!callback) {
    return {};
  }
  std::uint64_t const id = nextSubscriberId_++;
  subscribers_.push_back(Subscriber{id, std::move(callback)});
  if (!running_) {
    startTimer();
  }
  return ObserverHandle{id};
}

void AnimationClock::unsubscribe(ObserverHandle handle) {
  if (!handle.isValid()) {
    return;
  }
  std::erase_if(subscribers_, [&](Subscriber const& s) { return s.id == handle.id; });
  if (!needsTimer()) {
    stopTimer();
  }
}

void AnimationClock::onTick(std::int64_t deadlineNanos) {
  const double now = static_cast<double>(deadlineNanos) * 1e-9;
  AnimationTick const tick{deadlineNanos, now};

  std::vector<AnimationBase*> snapshot = active_;
  for (AnimationBase* p : snapshot) {
    if (!p) {
      continue;
    }
    if (!p->tick(now)) {
      unregisterAnimation(p);
    }
  }

  std::vector<Subscriber> subSnap = subscribers_;
  for (Subscriber const& s : subSnap) {
    if (s.callback) {
      s.callback(tick);
    }
  }

  if (!needsTimer()) {
    stopTimer();
  }
}

void AnimationClock::startTimer() {
  if (running_) {
    return;
  }
  if (!Application::hasInstance()) {
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
