#pragma once

/// \file Flux/Reactive/AnimationClock.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Observer.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace flux {

class AnimatedBase;
class EventQueue;

/// One tick from the shared ~60 Hz animation timer (`steady_clock` domain, same as `TimerEvent::deadlineNanos`).
struct AnimationTick {
  /// `steady_clock` time since epoch in nanoseconds (matches `TimerEvent::deadlineNanos`).
  std::int64_t deadlineNanos = 0;
  /// Monotonic time in seconds (same domain as `AnimatedBase::tick`).
  double nowSeconds = 0.;
};

class AnimationClock {
public:
  static AnimationClock& instance();

  void install(EventQueue& q);
  void shutdown();

  void registerAnimated(AnimatedBase* animated);
  void unregisterAnimated(AnimatedBase* animated);

  /// Subscribe to the shared animation tick. The callback must not assume a full frame will be presented — only call
  /// `Window::requestRepaint()` or `Application::requestRebuild()` when output actually changes.
  ObserverHandle subscribe(std::function<void(AnimationTick const&)> callback);
  void unsubscribe(ObserverHandle handle);

private:
  AnimationClock();

  bool needsTimer() const;
  void onTick(std::int64_t deadlineNanos);
  void startTimer();
  void stopTimer();

  struct Subscriber {
    std::uint64_t id = 0;
    std::function<void(AnimationTick const&)> callback;
  };

  std::vector<AnimatedBase*> active_;
  std::vector<Subscriber> subscribers_;
  std::uint64_t nextSubscriberId_ = 1;

  std::uint64_t timerId_ = 0;
  bool running_ = false;
  bool installed_ = false;
};

} // namespace flux
