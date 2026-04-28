#include "Effect.hpp"
#include "Signal.hpp"
#include "SmallFn.hpp"
#include "Test.hpp"

#include <array>
#include <functional>

using namespace fluxv5;

static void runSignalTests() {
  Signal<int> count(1);
  V5_CHECK(count.get() == 1);
  count.set(2);
  V5_CHECK(count.get() == 2);

  int effectRuns = 0;
  int observed = 0;
  Effect effect([&] {
    ++effectRuns;
    observed = count.get();
  });

  V5_CHECK(effectRuns == 1);
  V5_CHECK(observed == 2);

  count.set(2);
  V5_CHECK(effectRuns == 1);

  count.set(3);
  V5_CHECK(effectRuns == 2);
  V5_CHECK(observed == 3);

  int untracked = 0;
  Effect untrackedEffect([&] {
    untracked = untrack([&] {
      return count.get();
    });
  });
  V5_CHECK(untracked == 3);
  count.set(4);
  V5_CHECK(untracked == 3);

  int captured = 7;
  SmallFn<int(int)> small([captured](int value) {
    return captured + value;
  });
  V5_CHECK(small(5) == 12);
  V5_CHECK(!small.usesHeapStorage());

  auto largeCapture = [payload = std::array<int, 16>{}](int value) {
    return payload[0] + value;
  };
  SmallFn<int(int)> large(largeCapture);
  V5_CHECK(large(9) == 9);
  V5_CHECK(large.usesHeapStorage());
}

V5_TEST_MAIN(runSignalTests)
