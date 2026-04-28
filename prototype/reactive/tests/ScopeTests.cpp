#include "Effect.hpp"
#include "Scope.hpp"
#include "Signal.hpp"
#include "Test.hpp"

#include <vector>

using namespace fluxv5;

static void runScopeTests() {
  Signal<int> source(0);
  int runs = 0;
  std::vector<int> cleanupOrder;

  Scope scope;
  withOwner(scope, [&] {
    onCleanup([&] {
      cleanupOrder.push_back(1);
    });
    onCleanup([&] {
      cleanupOrder.push_back(2);
    });
    Effect([&] {
      (void)source.get();
      ++runs;
    });
  });

  V5_CHECK(runs == 1);
  source.set(1);
  V5_CHECK(runs == 2);

  scope.dispose();
  V5_CHECK(scope.disposed());
  V5_CHECK(cleanupOrder.size() == 2);
  V5_CHECK(cleanupOrder[0] == 2);
  V5_CHECK(cleanupOrder[1] == 1);

  source.set(2);
  V5_CHECK(runs == 2);

  Scope explicitOwner;
  Signal<int> ownedSignal = withOwner(explicitOwner, [] {
    return Signal<int>(42);
  });
  V5_CHECK(ownedSignal.get() == 42);
  explicitOwner.dispose();
  V5_CHECK(ownedSignal.disposed());
}

V5_TEST_MAIN(runScopeTests)
