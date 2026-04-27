#include <doctest/doctest.h>

#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

TEST_CASE("view modifiers infer bindables from mixed values and closures") {
  flux::Reactive::Signal<float> opacity{0.5f};

  auto sized = flux::Rectangle{}.size(22.f, [] { return 18.f; });
  auto filled = flux::Rectangle{}.fill(flux::Colors::red);
  auto faded = flux::Rectangle{}.opacity([opacity] { return opacity.get(); });

  CHECK(sized.modifiers() != nullptr);
  CHECK(filled.modifiers() != nullptr);
  CHECK(faded.modifiers() != nullptr);
}
