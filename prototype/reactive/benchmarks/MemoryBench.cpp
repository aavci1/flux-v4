#include "Effect.hpp"
#include "Signal.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace fluxv5;

int main() {
  constexpr int leafCount = 4096;
  Signal<int> source(0);
  std::vector<Effect> effects;
  effects.reserve(leafCount);

  for (int i = 0; i < leafCount; ++i) {
    effects.emplace_back([&, i] {
      (void)i;
      (void)source.get();
    });
  }

  source.set(1);
  auto liveBefore = detail::debugLiveLinkCount();
  detail::debugResetLinkAllocationCount();

  source.set(2);

  auto liveAfter = detail::debugLiveLinkCount();
  auto allocatedDuringSteadyState = detail::debugTotalLinkAllocations();
  double steadyStateAllocationsPerLeaf =
    static_cast<double>(allocatedDuringSteadyState) / leafCount;
  auto bytesPerReactiveLeaf = sizeof(detail::Link);

  std::cout << "reactive_leaf_link_bytes=" << bytesPerReactiveLeaf << "\n";
  std::cout << "steady_state_link_allocations_per_leaf="
            << steadyStateAllocationsPerLeaf << "\n";
  std::cout << "live_links_before=" << liveBefore << "\n";
  std::cout << "live_links_after=" << liveAfter << "\n";

  bool pass = bytesPerReactiveLeaf < 64 &&
              allocatedDuringSteadyState == 0 &&
              liveBefore == liveAfter;
  return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
