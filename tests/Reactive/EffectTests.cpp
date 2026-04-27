#include <Flux/Reactive/Animation.hpp>
#include <Flux/Reactive/Computed.hpp>
#include <Flux/Reactive/Effect.hpp>
#include <Flux/Reactive/Signal.hpp>

#include <doctest/doctest.h>

using namespace flux::Reactive;

TEST_CASE("Reactive Effect runs once per source write") {
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

TEST_CASE("Reactive Effect updates dynamic subscriptions") {
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

TEST_CASE("Reactive Effect reruns do not inherit ambient WithTransition") {
  Signal<int> trigger(0);
  flux::Animation<float> readAnimation{0.f};
  flux::Animation<float> writeAnimation{0.f};
  int runs = 0;
  float observed = -1.f;

  Effect effect([&] {
    ++runs;
    observed = readAnimation.get();
    if (trigger.get() > 0) {
      writeAnimation = 5.f;
    }
  });

  CHECK(runs == 1);
  CHECK(observed == doctest::Approx(0.f));
  CHECK_FALSE(writeAnimation.isRunning());

  {
    flux::WithTransition transition{flux::Transition::linear(10.f)};
    readAnimation = 1.f;
    CHECK(readAnimation.isRunning());
    CHECK(readAnimation.get() == doctest::Approx(0.f));

    trigger.set(1);
  }

  CHECK(runs == 2);
  CHECK(observed == doctest::Approx(0.f));
  CHECK_FALSE(writeAnimation.isRunning());
  CHECK(writeAnimation.get() == doctest::Approx(5.f));

  Signal<int> localTrigger(0);
  flux::Animation<float> scopedWrite{0.f};
  Effect scopedEffect([&] {
    if (localTrigger.get() > 0) {
      flux::WithTransition transition{flux::Transition::linear(10.f)};
      scopedWrite = 8.f;
    }
  });

  localTrigger.set(1);

  CHECK(scopedWrite.isRunning());
  CHECK(scopedWrite.get() == doctest::Approx(0.f));
}
