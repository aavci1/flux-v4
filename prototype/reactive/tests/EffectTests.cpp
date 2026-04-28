#include "Computed.hpp"
#include "Effect.hpp"
#include "Signal.hpp"
#include "Test.hpp"

using namespace fluxv5;

static void runEffectTests() {
  Signal<int> source(1);
  Computed<int> doubled([&] {
    return source.get() * 2;
  });

  int runs = 0;
  int observed = 0;
  Effect effect([&] {
    ++runs;
    observed = doubled.get();
  });

  V5_CHECK(runs == 1);
  V5_CHECK(observed == 2);

  source.set(5);
  V5_CHECK(runs == 2);
  V5_CHECK(observed == 10);

  effect.dispose();
  source.set(6);
  V5_CHECK(runs == 2);

  Signal<bool> chooseLeft(true);
  Signal<int> left(1);
  Signal<int> right(10);
  int dynamicObserved = 0;
  int dynamicRuns = 0;
  Effect dynamicEffect([&] {
    ++dynamicRuns;
    dynamicObserved = chooseLeft.get() ? left.get() : right.get();
  });

  V5_CHECK(dynamicObserved == 1);
  right.set(11);
  V5_CHECK(dynamicRuns == 1);

  chooseLeft.set(false);
  V5_CHECK(dynamicRuns == 2);
  V5_CHECK(dynamicObserved == 11);

  left.set(2);
  V5_CHECK(dynamicRuns == 2);
}

V5_TEST_MAIN(runEffectTests)
