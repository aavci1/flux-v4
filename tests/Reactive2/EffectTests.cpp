#include <Flux/Reactive2/Computed.hpp>
#include <Flux/Reactive2/Effect.hpp>
#include <Flux/Reactive2/Signal.hpp>

#include <doctest/doctest.h>

using namespace flux::Reactive2;

TEST_CASE("Reactive2 Effect runs once per source write") {
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

  CHECK(runs == 1);
  CHECK(observed == 2);

  source.set(5);
  CHECK(runs == 2);
  CHECK(observed == 10);

  effect.dispose();
  source.set(6);
  CHECK(runs == 2);
}

TEST_CASE("Reactive2 Effect updates dynamic subscriptions") {
  Signal<bool> chooseLeft(true);
  Signal<int> left(1);
  Signal<int> right(10);
  int observed = 0;
  int runs = 0;
  Effect effect([&] {
    ++runs;
    observed = chooseLeft.get() ? left.get() : right.get();
  });

  CHECK(observed == 1);
  right.set(11);
  CHECK(runs == 1);

  chooseLeft.set(false);
  CHECK(runs == 2);
  CHECK(observed == 11);

  left.set(2);
  CHECK(runs == 2);
}
