#pragma once

/// \file Flux/Reactive/AnimationClock.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Observer.hpp>
#include <Flux/Reactive/SmallFn.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace flux {

class AnimationBase;
class Application;
class EventQueue;
template<typename Fn>
void useFrame(Fn&& callback);

/// One tick from the shared frame pump (`steady_clock` domain, same as `FrameEvent::deadlineNanos`).
struct AnimationTick {
  /// `steady_clock` time since epoch in nanoseconds (matches `FrameEvent::deadlineNanos`).
  std::int64_t deadlineNanos = 0;
  /// Monotonic time in seconds (same domain as `AnimationBase::tick`).
  double nowSeconds = 0.;
};

enum class FrameAction {
  /// Unsubscribe the frame callback without forcing a redraw.
  Stop,
  /// Unsubscribe the frame callback after requesting one final redraw.
  StopAndRedraw,
  /// Keep the callback active without forcing a redraw.
  Continue,
  /// Keep the callback active and redraw all windows on this tick.
  ContinueAndRedraw,
};

class AnimationClock {
public:
  static AnimationClock& instance();
  static double nowSeconds();

  void install(EventQueue& q);
  void shutdown();

  void registerAnimation(AnimationBase* animation);
  void registerAnimation(std::shared_ptr<AnimationBase> animation);
  void unregisterAnimation(AnimationBase* animation);

#if defined(FLUX_TESTING)
  void testTick(double nowSeconds) { onTick(static_cast<std::int64_t>(nowSeconds * 1e9)); }
  std::size_t testOwnedAnimationCount() const { return ownedActive_.size(); }
#endif

private:
  friend class Application;
  template<typename Fn>
  friend void useFrame(Fn&& callback);

  AnimationClock();

  bool needsFramePump() const;
  /// Subscribe to the shared animation tick. Return a redraw action when output actually changes.
  ObserverHandle subscribe(Reactive::SmallFn<FrameAction(AnimationTick const&)> callback);
  void unsubscribe(ObserverHandle handle);
  void onTick(std::int64_t deadlineNanos);
  void startFramePump();
  void stopFramePump();

  struct Subscriber {
    std::uint64_t id = 0;
    Reactive::SmallFn<FrameAction(AnimationTick const&)> callback;
    bool active = true;
  };

  std::vector<AnimationBase*> active_;
  std::vector<std::shared_ptr<AnimationBase>> ownedActive_;
  std::vector<Subscriber> subscribers_;
  std::uint64_t nextSubscriberId_ = 1;

  bool running_ = false;
  bool installed_ = false;
  bool framePulseQueued_ = false;
  bool dispatchingSubscribers_ = false;
  bool subscribersNeedCompaction_ = false;
};

} // namespace flux
