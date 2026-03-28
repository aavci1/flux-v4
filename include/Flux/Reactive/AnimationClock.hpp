#pragma once

#include <cstdint>
#include <vector>

namespace flux {

class AnimatedBase;
class EventQueue;

class AnimationClock {
public:
  static AnimationClock& instance();

  void install(EventQueue& q);
  void shutdown();

  void registerAnimated(AnimatedBase* animated);
  void unregisterAnimated(AnimatedBase* animated);

private:
  AnimationClock();

  void onTick(std::int64_t deadlineNanos);
  void startTimer();
  void stopTimer();

  std::vector<AnimatedBase*> active_;
  std::uint64_t timerId_ = 0;
  bool running_ = false;
  bool installed_ = false;
};

} // namespace flux
