#include <doctest/doctest.h>

#define private public
#include <Flux/Reactive/Animation.hpp>
#undef private

using namespace flux;

TEST_CASE("Animation repeats across finite iterations") {
  Animation<float> value{0.f};
  value.play(10.f, AnimationOptions {
      .transition = Transition::linear(1.f),
      .repeat = 3,
      .autoreverse = false,
  });

  value.startTime_ = 100.0;

  CHECK(value.tick(100.50));
  CHECK(value.get() == doctest::Approx(5.f));

  CHECK(value.tick(101.00));
  CHECK(value.get() == doctest::Approx(0.f));

  CHECK(value.tick(101.50));
  CHECK(value.get() == doctest::Approx(5.f));

  CHECK_FALSE(value.tick(103.00));
  CHECK(value.get() == doctest::Approx(10.f));
  CHECK_FALSE(value.isRunning());
}

TEST_CASE("Animation autoreverse returns to its start on even iteration counts") {
  Animation<float> value{0.f};
  value.play(10.f, AnimationOptions {
      .transition = Transition::linear(1.f),
      .repeat = 2,
      .autoreverse = true,
  });

  value.startTime_ = 10.0;

  CHECK(value.tick(10.50));
  CHECK(value.get() == doctest::Approx(5.f));

  CHECK(value.tick(11.50));
  CHECK(value.get() == doctest::Approx(5.f));

  CHECK_FALSE(value.tick(12.00));
  CHECK(value.get() == doctest::Approx(0.f));
  CHECK_FALSE(value.isRunning());
}

TEST_CASE("Animation options preserve transition and playback configuration") {
  AnimationOptions const options {
      .transition = Transition::ease(0.4f).delayed(0.2f),
      .repeat = AnimationOptions::kRepeatForever,
      .autoreverse = true,
  };

  CHECK(options.transition.duration == doctest::Approx(0.4f));
  CHECK(options.transition.delay == doctest::Approx(0.2f));
  CHECK(options.repeat == AnimationOptions::kRepeatForever);
  CHECK(options.autoreverse);
}

TEST_CASE("Animation snaps to target when reduced motion is enabled") {
  Animation<float> value{0.f};
  value.play(10.f, AnimationOptions {
      .transition = Transition::linear(1.f),
      .repeat = AnimationOptions::kRepeatForever,
      .autoreverse = false,
  });

  value.startTime_ = 10.0;
  REQUIRE(value.tick(10.25));
  CHECK(value.get() == doctest::Approx(2.5f));
  CHECK(value.isRunning());

  value.setReducedMotion(true);

  CHECK(value.get() == doctest::Approx(10.f));
  CHECK_FALSE(value.isRunning());

  value.play(20.f, Transition::ease(0.5f).delayed(1.0f));
  CHECK(value.get() == doctest::Approx(20.f));
  CHECK_FALSE(value.isRunning());
}
