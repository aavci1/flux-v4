#include <Flux/Reactive2/Computed.hpp>
#include <Flux/Reactive2/Signal.hpp>

#include <doctest/doctest.h>

using namespace flux::Reactive2;

TEST_CASE("Reactive2 Computed recomputes lazily") {
  Signal<int> source(2);
  int runs = 0;
  Computed<int> doubled([&] {
    ++runs;
    return source.get() * 2;
  });

  CHECK(doubled.get() == 4);
  CHECK(runs == 1);

  source.set(3);
  CHECK(runs == 1);
  CHECK(doubled.get() == 6);
  CHECK(runs == 2);
}

TEST_CASE("Reactive2 Computed supports transitive and dynamic dependencies") {
  Signal<int> source(3);
  Computed<int> doubled([&] {
    return source.get() * 2;
  });
  Computed<int> plusOne([&] {
    return doubled.get() + 1;
  });

  CHECK(plusOne.get() == 7);
  source.set(4);
  CHECK(plusOne.get() == 9);

  Signal<bool> useLeft(true);
  Signal<int> left(10);
  Signal<int> right(20);
  int runs = 0;
  Computed<int> selected([&] {
    ++runs;
    return useLeft.get() ? left.get() : right.get();
  });

  CHECK(selected.get() == 10);
  right.set(21);
  CHECK(selected.get() == 10);
  CHECK(runs == 1);

  useLeft.set(false);
  CHECK(selected.get() == 21);
  CHECK(runs == 2);

  left.set(11);
  CHECK(selected.get() == 21);
  CHECK(runs == 2);
}
