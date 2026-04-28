#include "Computed.hpp"
#include "Effect.hpp"
#include "Signal.hpp"
#include "Test.hpp"

using namespace fluxv5;

static void runDiamondTests() {
  Signal<int> source(1);
  int bRuns = 0;
  int cRuns = 0;
  int dRuns = 0;
  int effectRuns = 0;
  int observed = 0;

  Computed<int> b([&] {
    ++bRuns;
    return source.get() + 1;
  });

  Computed<int> c([&] {
    ++cRuns;
    return source.get() + 2;
  });

  Computed<int> d([&] {
    ++dRuns;
    return b.get() + c.get();
  });

  Effect effect([&] {
    ++effectRuns;
    observed = d.get();
  });

  V5_CHECK(observed == 5);
  V5_CHECK(effectRuns == 1);
  V5_CHECK(dRuns == 1);

  source.set(2);
  V5_CHECK(observed == 7);
  V5_CHECK(effectRuns == 2);
  V5_CHECK(dRuns == 2);
  V5_CHECK(bRuns == 2);
  V5_CHECK(cRuns == 2);

  source.set(3);
  V5_CHECK(observed == 9);
  V5_CHECK(effectRuns == 3);
  V5_CHECK(dRuns == 3);
}

V5_TEST_MAIN(runDiamondTests)
