#pragma once

#include "UI/DebugFlags.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace flux::debug::perf {

enum class TimedMetric : std::uint8_t {
  ProcessReactiveUpdates = 0,
  IncrementalRebuild,
  SceneRender,
  CanvasPresent,
  DisplayLinkToPresent,
  Count,
};

struct BuildCounters {
  std::uint64_t resolvedNodes = 0;
  std::uint64_t materializedNodes = 0;
  std::uint64_t arrangedNodes = 0;
  std::uint64_t reusedNodes = 0;
};

struct ComponentKeyCounters {
  std::uint64_t copies = 0;
  std::uint64_t copiedIds = 0;
  std::uint64_t appends = 0;
  std::uint64_t appendedIds = 0;
  std::uint64_t hashCalls = 0;
  std::uint64_t hashedIds = 0;
  std::uint64_t equalityCalls = 0;
  std::uint64_t equalityIds = 0;
  std::uint64_t prefixCalls = 0;
  std::uint64_t prefixIds = 0;
  std::uint64_t heapGrowths = 0;
  std::uint64_t heapCapacity = 0;
};

namespace detail {

struct IntervalCounters {
  std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();
  std::uint64_t frames = 0;
  std::uint64_t builds = 0;
  BuildCounters build{};
  ComponentKeyCounters componentKeys{};
  std::uint64_t preparedPrepareCalls = 0;
  std::uint64_t preparedReplayCalls = 0;
  std::array<std::uint64_t, static_cast<std::size_t>(TimedMetric::Count)> durationsNs{};

  void reset(std::chrono::steady_clock::time_point now) {
    startedAt = now;
    frames = 0;
    builds = 0;
    build = {};
    componentKeys = {};
    preparedPrepareCalls = 0;
    preparedReplayCalls = 0;
    durationsNs.fill(0);
  }
};

inline IntervalCounters& counters() {
  static IntervalCounters value{};
  return value;
}

inline double perFrame(std::uint64_t total, std::uint64_t frames) {
  if (frames == 0) {
    return 0.0;
  }
  return static_cast<double>(total) / static_cast<double>(frames);
}

inline double nanosToMillis(std::uint64_t totalNs) {
  return static_cast<double>(totalNs) / 1'000'000.0;
}

inline void logIfReady() {
  IntervalCounters& interval = counters();
  auto const now = std::chrono::steady_clock::now();
  auto const elapsed = now - interval.startedAt;
  if (elapsed < std::chrono::seconds(1)) {
    return;
  }

  double const seconds = std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();
  std::fprintf(
      stderr,
      "[flux:perf] %.2fs frames=%llu builds=%llu "
      "resolved=%llu(%.1f/f) materialized=%llu(%.1f/f) arranged=%llu(%.1f/f) reused=%llu(%.1f/f) "
      "ck copy=%llu/%lluid append=%llu/%lluid hash=%llu/%lluid eq=%llu/%lluid prefix=%llu/%lluid grow=%llu "
      "prepare=%llu(%.2f/f) replay=%llu(%.2f/f) "
      "ms reactive=%.2f(%.2f/f) incremental=%.2f(%.2f/f) render=%.2f(%.2f/f) present=%.2f(%.2f/f) frameBudget=%.2f(%.2f/f)\n",
      seconds,
      static_cast<unsigned long long>(interval.frames),
      static_cast<unsigned long long>(interval.builds),
      static_cast<unsigned long long>(interval.build.resolvedNodes),
      perFrame(interval.build.resolvedNodes, interval.frames),
      static_cast<unsigned long long>(interval.build.materializedNodes),
      perFrame(interval.build.materializedNodes, interval.frames),
      static_cast<unsigned long long>(interval.build.arrangedNodes),
      perFrame(interval.build.arrangedNodes, interval.frames),
      static_cast<unsigned long long>(interval.build.reusedNodes),
      perFrame(interval.build.reusedNodes, interval.frames),
      static_cast<unsigned long long>(interval.componentKeys.copies),
      static_cast<unsigned long long>(interval.componentKeys.copiedIds),
      static_cast<unsigned long long>(interval.componentKeys.appends),
      static_cast<unsigned long long>(interval.componentKeys.appendedIds),
      static_cast<unsigned long long>(interval.componentKeys.hashCalls),
      static_cast<unsigned long long>(interval.componentKeys.hashedIds),
      static_cast<unsigned long long>(interval.componentKeys.equalityCalls),
      static_cast<unsigned long long>(interval.componentKeys.equalityIds),
      static_cast<unsigned long long>(interval.componentKeys.prefixCalls),
      static_cast<unsigned long long>(interval.componentKeys.prefixIds),
      static_cast<unsigned long long>(interval.componentKeys.heapGrowths),
      static_cast<unsigned long long>(interval.preparedPrepareCalls),
      perFrame(interval.preparedPrepareCalls, interval.frames),
      static_cast<unsigned long long>(interval.preparedReplayCalls),
      perFrame(interval.preparedReplayCalls, interval.frames),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::ProcessReactiveUpdates)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::ProcessReactiveUpdates)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::IncrementalRebuild)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::IncrementalRebuild)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::SceneRender)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::SceneRender)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::CanvasPresent)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::CanvasPresent)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::DisplayLinkToPresent)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::DisplayLinkToPresent)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)));

  interval.reset(now);
}

} // namespace detail

inline bool enabled() {
  return perfEnabled();
}

inline void recordBuildCounters(BuildCounters const& build) {
  if (!enabled()) {
    return;
  }
  detail::IntervalCounters& interval = detail::counters();
  ++interval.builds;
  interval.build.resolvedNodes += build.resolvedNodes;
  interval.build.materializedNodes += build.materializedNodes;
  interval.build.arrangedNodes += build.arrangedNodes;
  interval.build.reusedNodes += build.reusedNodes;
}

inline void recordPreparedPrepareCall() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().preparedPrepareCalls;
}

inline void recordPreparedReplayCall() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().preparedReplayCalls;
}

inline void recordDuration(TimedMetric metric, std::chrono::nanoseconds elapsed) {
  if (!enabled()) {
    return;
  }
  detail::counters().durationsNs[static_cast<std::size_t>(metric)] +=
      static_cast<std::uint64_t>(elapsed.count());
}

inline void recordComponentKeyCopy(std::uint64_t copiedIds) {
  if (!enabled()) {
    return;
  }
  auto& counters = detail::counters().componentKeys;
  ++counters.copies;
  counters.copiedIds += copiedIds;
}

inline void recordComponentKeyAppend(std::uint64_t resultingIds) {
  if (!enabled()) {
    return;
  }
  auto& counters = detail::counters().componentKeys;
  ++counters.appends;
  counters.appendedIds += resultingIds;
}

inline void recordComponentKeyHash(std::uint64_t hashedIds) {
  if (!enabled()) {
    return;
  }
  auto& counters = detail::counters().componentKeys;
  ++counters.hashCalls;
  counters.hashedIds += hashedIds;
}

inline void recordComponentKeyEquality(std::uint64_t comparedIds) {
  if (!enabled()) {
    return;
  }
  auto& counters = detail::counters().componentKeys;
  ++counters.equalityCalls;
  counters.equalityIds += comparedIds;
}

inline void recordComponentKeyPrefixCompare(std::uint64_t comparedIds) {
  if (!enabled()) {
    return;
  }
  auto& counters = detail::counters().componentKeys;
  ++counters.prefixCalls;
  counters.prefixIds += comparedIds;
}

inline void recordComponentKeyHeapGrowth(std::uint64_t capacity) {
  if (!enabled()) {
    return;
  }
  auto& counters = detail::counters().componentKeys;
  ++counters.heapGrowths;
  counters.heapCapacity += capacity;
}

inline void recordPresentedFrame() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().frames;
  detail::logIfReady();
}

class ScopedTimer {
public:
  explicit ScopedTimer(TimedMetric metric)
      : metric_(metric), enabled_(perf::enabled()),
        startedAt_(enabled_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}) {}

  ~ScopedTimer() {
    if (!enabled_) {
      return;
    }
    recordDuration(metric_, std::chrono::steady_clock::now() - startedAt_);
  }

  ScopedTimer(ScopedTimer const&) = delete;
  ScopedTimer& operator=(ScopedTimer const&) = delete;

private:
  TimedMetric metric_;
  bool enabled_ = false;
  std::chrono::steady_clock::time_point startedAt_{};
};

} // namespace flux::debug::perf
