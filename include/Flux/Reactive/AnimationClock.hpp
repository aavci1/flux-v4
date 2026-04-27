#pragma once

/// \file Flux/Reactive/AnimationClock.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Observer.hpp>
#include <Flux/Reactive/SmallFn.hpp>

#include <cstdint>
#include <vector>

namespace flux {

class AnimationBase;
class Application;
class EventQueue;
template<typename Fn>
void useAnimationFrame(Fn&& callback);

/// One tick from the shared frame pump (`steady_clock` domain, same as `FrameEvent::deadlineNanos`).
struct AnimationTick {
  /// `steady_clock` time since epoch in nanoseconds (matches `FrameEvent::deadlineNanos`).
  std::int64_t deadlineNanos = 0;
  /// Monotonic time in seconds (same domain as `AnimationBase::tick`).
  double nowSeconds = 0.;
};

class AnimationClock {
public:
  static AnimationClock& instance();

  void install(EventQueue& q);
  void shutdown();

  void registerAnimation(AnimationBase* animation);
  void unregisterAnimation(AnimationBase* animation);

private:
  friend class Application;
  template<typename Fn>
  friend void useAnimationFrame(Fn&& callback);

  AnimationClock();

  bool needsFramePump() const;
  /// Subscribe to the shared animation tick. The callback must not assume a full frame will be presented — only call
  /// `Window::requestRedraw()` when output actually changes.
  ObserverHandle subscribe(Reactive::SmallFn<void(AnimationTick const&)> callback);
  void unsubscribe(ObserverHandle handle);
  void onTick(std::int64_t deadlineNanos);
  void startFramePump();
  void stopFramePump();

  struct Subscriber {
    std::uint64_t id = 0;
    Reactive::SmallFn<void(AnimationTick const&)> callback;
    bool active = true;
  };

  std::vector<AnimationBase*> active_;
  std::vector<Subscriber> subscribers_;
  std::uint64_t nextSubscriberId_ = 1;

  bool running_ = false;
  bool installed_ = false;
  bool framePulseQueued_ = false;
  bool dispatchingSubscribers_ = false;
  bool subscribersNeedCompaction_ = false;
};

} // namespace flux
