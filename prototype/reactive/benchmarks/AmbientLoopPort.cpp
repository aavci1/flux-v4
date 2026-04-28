#include "Effect.hpp"
#include "Show.hpp"
#include "Signal.hpp"
#include "ToyScene.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

using namespace fluxv5;

static std::size_t residentBytes() {
#if defined(__APPLE__)
  mach_task_basic_info_data_t info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  kern_return_t result = task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
    reinterpret_cast<task_info_t>(&info), &count);
  if (result == KERN_SUCCESS) {
    return static_cast<std::size_t>(info.resident_size);
  }
#endif
  return 0;
}

static constexpr bool sanitizerBuild() {
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || \
    __has_feature(undefined_behavior_sanitizer)
  return true;
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__) || \
    defined(__SANITIZE_UNDEFINED__)
  return true;
#else
  return false;
#endif
}

int main() {
  constexpr int barCount = 5;
  constexpr int frames = 60 * 30;

  Signal<float> phase(0.0f);
  Signal<bool> reducedMotion(false);
  auto root = makeToyNode();
  std::vector<std::shared_ptr<ToyNode>> bars;
  bars.reserve(barCount);

  Scope scope;
  int branchRuns = 0;
  auto predicate = [&] {
    return reducedMotion.get();
  };
  auto thenFn = [&] {
    ++branchRuns;
    return std::string("reduced");
  };
  auto elseFn = [&] {
    ++branchRuns;
    return std::string("animated");
  };
  using ShowType = decltype(Show(predicate, thenFn, elseFn));
  std::optional<ShowType> show;

  withOwner(scope, [&] {
    for (int i = 0; i < barCount; ++i) {
      auto bar = makeToyNode();
      root->addChild(bar);
      bars.push_back(bar);
      Effect([bar, i, phase] {
        float wave = std::sin(phase.get() + static_cast<float>(i) * 0.55f);
        float emphasis = (wave + 1.0f) * 0.5f;
        bar->setSize(22.0f, 18.0f + emphasis * 34.0f);
        bar->setOpacity(0.55f + emphasis * 0.45f);
      });
    }

    show.emplace(predicate, thenFn, elseFn);
    (void)show->output();
  });

  phase.set(0.1f);
  auto liveLinksBefore = detail::debugLiveLinkCount();
  auto rssBefore = residentBytes();
  detail::debugResetLinkAllocationCount();

  auto start = std::chrono::steady_clock::now();
  for (int frame = 0; frame < frames; ++frame) {
    phase.set(static_cast<float>(frame) / 60.0f);
  }
  auto end = std::chrono::steady_clock::now();

  auto liveLinksAfter = detail::debugLiveLinkCount();
  auto rssAfter = residentBytes();
  auto newLinks = detail::debugTotalLinkAllocations();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  double usPerFrame = static_cast<double>(elapsed.count()) / frames;

  float totalHeight = 0.0f;
  for (auto const& bar : bars) {
    totalHeight += bar->height;
  }

  std::cout << "ambient_loop_us_per_frame=" << usPerFrame << "\n";
  std::cout << "ambient_loop_new_links_after_warmup=" << newLinks << "\n";
  std::cout << "ambient_loop_live_links_before=" << liveLinksBefore << "\n";
  std::cout << "ambient_loop_live_links_after=" << liveLinksAfter << "\n";
  std::cout << "ambient_loop_rss_before=" << rssBefore << "\n";
  std::cout << "ambient_loop_rss_after=" << rssAfter << "\n";
  std::cout << "ambient_loop_rss_delta="
            << (rssAfter >= rssBefore ? rssAfter - rssBefore : rssBefore - rssAfter)
            << "\n";
  std::cout << "ambient_loop_branch_runs=" << branchRuns << "\n";
  std::cout << "ambient_loop_total_height=" << totalHeight << "\n";

  bool rssStable = sanitizerBuild() || rssBefore == 0 || rssAfter == 0 ||
                   (rssAfter >= rssBefore ? rssAfter - rssBefore : rssBefore - rssAfter)
                     <= 100 * 1024;
  bool pass = newLinks == 0 && liveLinksBefore == liveLinksAfter &&
              rssStable && usPerFrame < 16'667.0 && branchRuns == 1 &&
              totalHeight > 0.0f;
  return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
