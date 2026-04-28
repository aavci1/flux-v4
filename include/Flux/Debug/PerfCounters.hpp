#pragma once

#include <Flux/Debug/DebugFlags.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace flux::debug::perf {

enum class TimedMetric : std::uint8_t {
  ProcessReactiveUpdates = 0,
  SceneRender,
  CanvasPresent,
  CanvasDrawableWait,
  DisplayLinkToPresent,
  Count,
};

enum class RenderCounterKind : std::uint8_t {
  Rect = 0,
  Image,
  Path,
  Glyph,
  Count,
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

struct RenderCounters {
  std::array<std::uint64_t, static_cast<std::size_t>(RenderCounterKind::Count)> ops{};
  std::array<std::uint64_t, static_cast<std::size_t>(RenderCounterKind::Count)> drawCalls{};
  std::array<std::uint64_t, static_cast<std::size_t>(RenderCounterKind::Count)> uploadBytes{};
  std::uint64_t opOrderEntries = 0;
  std::uint64_t pathVertices = 0;
  std::uint64_t glyphVertices = 0;
  std::uint64_t recorderCapacityGrowths = 0;
  std::uint64_t recorderCapacityGrowthBytes = 0;
};

struct SceneCounters {
  std::uint64_t renderPasses = 0;
  std::uint64_t nodesVisited = 0;
  std::uint64_t groupsVisited = 0;
  std::uint64_t leavesVisited = 0;
  std::uint64_t quickRejects = 0;
  std::uint64_t liveLeafRenders = 0;
  std::uint64_t preparedReplaySuccesses = 0;
  std::uint64_t preparedReplayFailures = 0;
};

struct TextCounters {
  std::uint64_t layoutCalls = 0;
  std::uint64_t layoutCacheHits = 0;
  std::uint64_t layoutCacheMisses = 0;
  std::uint64_t paragraphVariantHits = 0;
  std::uint64_t paragraphVariantMisses = 0;
};

namespace detail {

struct IntervalCounters {
  std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();
  std::uint64_t frames = 0;
  ComponentKeyCounters componentKeys{};
  RenderCounters render{};
  SceneCounters scene{};
  TextCounters text{};
  std::uint64_t preparedPrepareCalls = 0;
  std::uint64_t preparedReplayCalls = 0;
  std::array<std::uint64_t, static_cast<std::size_t>(TimedMetric::Count)> durationsNs{};

  void reset(std::chrono::steady_clock::time_point now) {
    startedAt = now;
    frames = 0;
    componentKeys = {};
    render = {};
    scene = {};
    text = {};
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
      "[flux:perf] %.2fs frames=%llu "
      "ck copy=%llu/%lluid append=%llu/%lluid hash=%llu/%lluid eq=%llu/%lluid prefix=%llu/%lluid grow=%llu "
      "prepare=%llu(%.2f/f) replay=%llu(%.2f/f) "
      "ms reactive=%.2f(%.2f/f) render=%.2f(%.2f/f) present=%.2f(%.2f/f) drawableWait=%.2f(%.2f/f) frameBudget=%.2f(%.2f/f)\n",
      seconds,
      static_cast<unsigned long long>(interval.frames),
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
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::SceneRender)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::SceneRender)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::CanvasPresent)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::CanvasPresent)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::CanvasDrawableWait)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::CanvasDrawableWait)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::DisplayLinkToPresent)]),
      nanosToMillis(interval.durationsNs[static_cast<std::size_t>(TimedMetric::DisplayLinkToPresent)]) /
          (interval.frames == 0 ? 1.0 : static_cast<double>(interval.frames)));

  std::fprintf(
      stderr,
      "[flux:perf:render] %.2fs frames=%llu "
      "scene passes=%llu(%.2f/f) nodes=%llu(%.2f/f) groups=%llu(%.2f/f) leaves=%llu(%.2f/f) "
      "reject=%llu live=%llu replayOk=%llu replayFail=%llu "
      "ops rect=%llu(%.2f/f) image=%llu(%.2f/f) path=%llu(%.2f/f) glyph=%llu(%.2f/f) order=%llu(%.2f/f) "
      "draw rect=%llu(%.2f/f) image=%llu(%.2f/f) path=%llu(%.2f/f) glyph=%llu(%.2f/f) "
      "uploadKB rect=%.1f(%.2f/f) image=%.1f(%.2f/f) path=%.1f(%.2f/f) glyph=%.1f(%.2f/f) "
      "verts path=%llu(%.2f/f) glyph=%llu(%.2f/f) "
      "recorderGrow=%llu growKB=%.1f "
      "text layout=%llu hit=%llu miss=%llu paraHit=%llu paraMiss=%llu\n",
      seconds,
      static_cast<unsigned long long>(interval.frames),
      static_cast<unsigned long long>(interval.scene.renderPasses),
      perFrame(interval.scene.renderPasses, interval.frames),
      static_cast<unsigned long long>(interval.scene.nodesVisited),
      perFrame(interval.scene.nodesVisited, interval.frames),
      static_cast<unsigned long long>(interval.scene.groupsVisited),
      perFrame(interval.scene.groupsVisited, interval.frames),
      static_cast<unsigned long long>(interval.scene.leavesVisited),
      perFrame(interval.scene.leavesVisited, interval.frames),
      static_cast<unsigned long long>(interval.scene.quickRejects),
      static_cast<unsigned long long>(interval.scene.liveLeafRenders),
      static_cast<unsigned long long>(interval.scene.preparedReplaySuccesses),
      static_cast<unsigned long long>(interval.scene.preparedReplayFailures),
      static_cast<unsigned long long>(interval.render.ops[static_cast<std::size_t>(RenderCounterKind::Rect)]),
      perFrame(interval.render.ops[static_cast<std::size_t>(RenderCounterKind::Rect)], interval.frames),
      static_cast<unsigned long long>(interval.render.ops[static_cast<std::size_t>(RenderCounterKind::Image)]),
      perFrame(interval.render.ops[static_cast<std::size_t>(RenderCounterKind::Image)], interval.frames),
      static_cast<unsigned long long>(interval.render.ops[static_cast<std::size_t>(RenderCounterKind::Path)]),
      perFrame(interval.render.ops[static_cast<std::size_t>(RenderCounterKind::Path)], interval.frames),
      static_cast<unsigned long long>(interval.render.ops[static_cast<std::size_t>(RenderCounterKind::Glyph)]),
      perFrame(interval.render.ops[static_cast<std::size_t>(RenderCounterKind::Glyph)], interval.frames),
      static_cast<unsigned long long>(interval.render.opOrderEntries),
      perFrame(interval.render.opOrderEntries, interval.frames),
      static_cast<unsigned long long>(interval.render.drawCalls[static_cast<std::size_t>(RenderCounterKind::Rect)]),
      perFrame(interval.render.drawCalls[static_cast<std::size_t>(RenderCounterKind::Rect)], interval.frames),
      static_cast<unsigned long long>(interval.render.drawCalls[static_cast<std::size_t>(RenderCounterKind::Image)]),
      perFrame(interval.render.drawCalls[static_cast<std::size_t>(RenderCounterKind::Image)], interval.frames),
      static_cast<unsigned long long>(interval.render.drawCalls[static_cast<std::size_t>(RenderCounterKind::Path)]),
      perFrame(interval.render.drawCalls[static_cast<std::size_t>(RenderCounterKind::Path)], interval.frames),
      static_cast<unsigned long long>(interval.render.drawCalls[static_cast<std::size_t>(RenderCounterKind::Glyph)]),
      perFrame(interval.render.drawCalls[static_cast<std::size_t>(RenderCounterKind::Glyph)], interval.frames),
      static_cast<double>(interval.render.uploadBytes[static_cast<std::size_t>(RenderCounterKind::Rect)]) / 1024.0,
      perFrame(interval.render.uploadBytes[static_cast<std::size_t>(RenderCounterKind::Rect)], interval.frames) /
          1024.0,
      static_cast<double>(interval.render.uploadBytes[static_cast<std::size_t>(RenderCounterKind::Image)]) / 1024.0,
      perFrame(interval.render.uploadBytes[static_cast<std::size_t>(RenderCounterKind::Image)], interval.frames) /
          1024.0,
      static_cast<double>(interval.render.uploadBytes[static_cast<std::size_t>(RenderCounterKind::Path)]) / 1024.0,
      perFrame(interval.render.uploadBytes[static_cast<std::size_t>(RenderCounterKind::Path)], interval.frames) /
          1024.0,
      static_cast<double>(interval.render.uploadBytes[static_cast<std::size_t>(RenderCounterKind::Glyph)]) / 1024.0,
      perFrame(interval.render.uploadBytes[static_cast<std::size_t>(RenderCounterKind::Glyph)], interval.frames) /
          1024.0,
      static_cast<unsigned long long>(interval.render.pathVertices),
      perFrame(interval.render.pathVertices, interval.frames),
      static_cast<unsigned long long>(interval.render.glyphVertices),
      perFrame(interval.render.glyphVertices, interval.frames),
      static_cast<unsigned long long>(interval.render.recorderCapacityGrowths),
      static_cast<double>(interval.render.recorderCapacityGrowthBytes) / 1024.0,
      static_cast<unsigned long long>(interval.text.layoutCalls),
      static_cast<unsigned long long>(interval.text.layoutCacheHits),
      static_cast<unsigned long long>(interval.text.layoutCacheMisses),
      static_cast<unsigned long long>(interval.text.paragraphVariantHits),
      static_cast<unsigned long long>(interval.text.paragraphVariantMisses));

  interval.reset(now);
}

} // namespace detail

inline bool enabled() {
  return perfEnabled();
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

inline void recordPreparedReplayResult(bool success) {
  if (!enabled()) {
    return;
  }
  if (success) {
    ++detail::counters().scene.preparedReplaySuccesses;
  } else {
    ++detail::counters().scene.preparedReplayFailures;
  }
}

inline void recordSceneRenderPass() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().scene.renderPasses;
}

inline void recordSceneNodeVisit(bool group) {
  if (!enabled()) {
    return;
  }
  auto& scene = detail::counters().scene;
  ++scene.nodesVisited;
  if (group) {
    ++scene.groupsVisited;
  } else {
    ++scene.leavesVisited;
  }
}

inline void recordSceneQuickReject() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().scene.quickRejects;
}

inline void recordLiveLeafRender() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().scene.liveLeafRenders;
}

inline void recordFrameOps(std::uint64_t rectOps, std::uint64_t imageOps, std::uint64_t pathOps,
                           std::uint64_t glyphOps, std::uint64_t orderEntries,
                           std::uint64_t pathVertices, std::uint64_t glyphVertices) {
  if (!enabled()) {
    return;
  }
  auto& render = detail::counters().render;
  render.ops[static_cast<std::size_t>(RenderCounterKind::Rect)] += rectOps;
  render.ops[static_cast<std::size_t>(RenderCounterKind::Image)] += imageOps;
  render.ops[static_cast<std::size_t>(RenderCounterKind::Path)] += pathOps;
  render.ops[static_cast<std::size_t>(RenderCounterKind::Glyph)] += glyphOps;
  render.opOrderEntries += orderEntries;
  render.pathVertices += pathVertices;
  render.glyphVertices += glyphVertices;
}

inline void recordDrawCall(RenderCounterKind kind) {
  if (!enabled()) {
    return;
  }
  ++detail::counters().render.drawCalls[static_cast<std::size_t>(kind)];
}

inline void recordUploadBytes(RenderCounterKind kind, std::uint64_t bytes) {
  if (!enabled()) {
    return;
  }
  detail::counters().render.uploadBytes[static_cast<std::size_t>(kind)] += bytes;
}

inline void recordRecorderCapacityGrowth(std::uint64_t bytes) {
  if (!enabled()) {
    return;
  }
  auto& render = detail::counters().render;
  ++render.recorderCapacityGrowths;
  render.recorderCapacityGrowthBytes += bytes;
}

inline void recordTextLayoutCall() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().text.layoutCalls;
}

inline void recordTextLayoutCacheHit() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().text.layoutCacheHits;
}

inline void recordTextLayoutCacheMiss() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().text.layoutCacheMisses;
}

inline void recordTextParagraphVariantHit() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().text.paragraphVariantHits;
}

inline void recordTextParagraphVariantMiss() {
  if (!enabled()) {
    return;
  }
  ++detail::counters().text.paragraphVariantMisses;
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
