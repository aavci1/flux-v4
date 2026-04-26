#include <Flux/Reactive2/Effect.hpp>
#include <Flux/Reactive2/Scope.hpp>
#include <Flux/Reactive2/Signal.hpp>

#include <doctest/doctest.h>
#include <vector>

using namespace flux::Reactive2;

TEST_CASE("Reactive2 Scope owns effects and cleanup callbacks") {
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

  CHECK(runs == 1);
  source.set(1);
  CHECK(runs == 2);

  scope.dispose();
  CHECK(scope.disposed());
  REQUIRE(cleanupOrder.size() == 2);
  CHECK(cleanupOrder[0] == 2);
  CHECK(cleanupOrder[1] == 1);

  source.set(2);
  CHECK(runs == 2);
}

TEST_CASE("Reactive2 withOwner explicitly owns created signals") {
  Scope owner;
  Signal<int> ownedSignal = withOwner(owner, [] {
    return Signal<int>(42);
  });

  CHECK(ownedSignal.get() == 42);
  owner.dispose();
  CHECK(ownedSignal.disposed());
}
