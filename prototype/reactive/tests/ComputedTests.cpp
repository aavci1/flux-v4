#include "Computed.hpp"
#include "Signal.hpp"
#include "Test.hpp"

using namespace fluxv5;

static void runComputedTests() {
  Signal<int> source(2);
  int doubleRuns = 0;
  Computed<int> doubled([&] {
    ++doubleRuns;
    return source.get() * 2;
  });

  V5_CHECK(doubled.get() == 4);
  V5_CHECK(doubleRuns == 1);

  source.set(3);
  V5_CHECK(doubleRuns == 1);
  V5_CHECK(doubled.get() == 6);
  V5_CHECK(doubleRuns == 2);

  Computed<int> plusOne([&] {
    return doubled.get() + 1;
  });
  V5_CHECK(plusOne.get() == 7);
  source.set(4);
  V5_CHECK(plusOne.get() == 9);

  Signal<bool> useLeft(true);
  Signal<int> left(10);
  Signal<int> right(20);
  int dynamicRuns = 0;
  Computed<int> selected([&] {
    ++dynamicRuns;
    return useLeft.get() ? left.get() : right.get();
  });

  V5_CHECK(selected.get() == 10);
  V5_CHECK(dynamicRuns == 1);

  right.set(21);
  V5_CHECK(dynamicRuns == 1);
  V5_CHECK(selected.get() == 10);

  useLeft.set(false);
  V5_CHECK(selected.get() == 21);
  V5_CHECK(dynamicRuns == 2);

  left.set(11);
  V5_CHECK(selected.get() == 21);
  V5_CHECK(dynamicRuns == 2);
}

V5_TEST_MAIN(runComputedTests)
