#pragma once

/// \file Flux/Reactive/AnimationClock.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Observer.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace flux {

class AnimationBase;
class Application;
class EventQueue;

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

  /// Subscribe to the shared animation tick. The callback must not assume a full frame will be presented — only call
  /// `Window::requestRedraw()` or `Application::markReactiveDirty()` when output actually changes.
  ObserverHandle subscribe(std::function<void(AnimationTick const&)> callback);
  void unsubscribe(ObserverHandle handle);

private:
  friend class Application;

  AnimationClock();

  bool needsFramePump() const;
  void onTick(std::int64_t deadlineNanos);
  void startFramePump();
  void stopFramePump();

  struct Subscriber {
    std::uint64_t id = 0;
    std::function<void(AnimationTick const&)> callback;
  };

  std::vector<AnimationBase*> active_;
  std::vector<Subscriber> subscribers_;
  std::uint64_t nextSubscriberId_ = 1;

  bool running_ = false;
  bool installed_ = false;
  bool framePulseQueued_ = false;
};

} // namespace flux
