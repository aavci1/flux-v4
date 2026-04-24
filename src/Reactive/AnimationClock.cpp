#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/Reactive/Animation.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace flux {

namespace {

struct AnimationFramePulse {
  std::int64_t deadlineNanos = 0;
};

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
  q.on<FrameEvent>([this, &q](FrameEvent const& e) {
    if (!running_ || framePulseQueued_) {
      return;
    }
    framePulseQueued_ = true;
    q.post(AnimationFramePulse{.deadlineNanos = e.deadlineNanos});
  });
  q.on<AnimationFramePulse>([this](AnimationFramePulse const& pulse) {
    framePulseQueued_ = false;
    if (!running_) {
      return;
    }
    onTick(pulse.deadlineNanos);
    if (needsFramePump()) {
      startFramePump();
    }
  });
  if (needsFramePump() && !running_) {
    startFramePump();
  }
}

void AnimationClock::shutdown() {
  stopFramePump();
  active_.clear();
  subscribers_.clear();
  nextSubscriberId_ = 1;
  framePulseQueued_ = false;
  installed_ = false;
}

bool AnimationClock::needsFramePump() const {
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
    startFramePump();
  }
}

void AnimationClock::unregisterAnimation(AnimationBase* animation) {
  if (!animation) {
    return;
  }
  std::erase(active_, animation);
  if (!needsFramePump()) {
    stopFramePump();
  }
}

ObserverHandle AnimationClock::subscribe(std::function<void(AnimationTick const&)> callback) {
  if (!callback) {
    return {};
  }
  std::uint64_t const id = nextSubscriberId_++;
  subscribers_.push_back(Subscriber{id, std::move(callback)});
  if (!running_) {
    startFramePump();
  }
  return ObserverHandle{id};
}

void AnimationClock::unsubscribe(ObserverHandle handle) {
  if (!handle.isValid()) {
    return;
  }
  std::erase_if(subscribers_, [&](Subscriber const& s) { return s.id == handle.id; });
  if (!needsFramePump()) {
    stopFramePump();
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

  if (!needsFramePump()) {
    stopFramePump();
  }
}

void AnimationClock::startFramePump() {
  if (!Application::hasInstance()) {
    return;
  }
  if (!running_) {
    running_ = true;
  }
  Application::instance().requestAnimationFrames();
}

void AnimationClock::stopFramePump() {
  if (!running_) {
    return;
  }
  running_ = false;
  framePulseQueued_ = false;
}

} // namespace flux
