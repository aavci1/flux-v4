#include <doctest/doctest.h>

#define private public
#include <Flux/Reactive/Animation.hpp>
#undef private

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Toggle.hpp>

#include <memory>
#include <string_view>
#include <vector>

using namespace flux;

namespace {

class FakeTextSystem final : public TextSystem {
public:
  std::shared_ptr<TextLayout const>
  layout(AttributedString const&, float, TextLayoutOptions const&) override {
    return std::make_shared<TextLayout>();
  }

  std::shared_ptr<TextLayout const>
  layout(std::string_view, Font const&, Color const&, float, TextLayoutOptions const&) override {
    return std::make_shared<TextLayout>();
  }

  Size measure(AttributedString const&, float, TextLayoutOptions const&) override {
    return {0.f, 0.f};
  }

  Size measure(std::string_view, Font const&, Color const&, float,
               TextLayoutOptions const&) override {
    return {0.f, 0.f};
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float,
                                           std::uint32_t& outWidth,
                                           std::uint32_t& outHeight,
                                           Point& outBearing) override {
    outWidth = 0;
    outHeight = 0;
    outBearing = {};
    return {};
  }
};

EnvironmentLayer testEnvironment() {
  EnvironmentLayer environment;
  environment.set(Theme::light());
  return environment;
}

scenegraph::RectNode const* findMovedThumb(scenegraph::SceneNode const& node) {
  if (node.kind() == scenegraph::SceneNodeKind::Rect) {
    auto const& rect = static_cast<scenegraph::RectNode const&>(node);
    Size const size = rect.size();
    Point const position = rect.position();
    if (size.width == doctest::Approx(18.f) &&
        size.height == doctest::Approx(18.f) &&
        position.x > 0.f) {
      return &rect;
    }
  }
  for (auto const& child : node.children()) {
    if (child) {
      if (auto const* found = findMovedThumb(*child)) {
        return found;
      }
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("Animation repeats across finite iterations") {
  Animation<float> value{0.f};
  value.play(10.f, AnimationOptions {
      .transition = Transition::linear(1.f),
      .repeat = 3,
      .autoreverse = false,
  });

  value.state_->startTime = 100.0;

  CHECK(value.state_->tick(100.50));
  CHECK(value.get() == doctest::Approx(5.f));

  CHECK(value.state_->tick(101.00));
  CHECK(value.get() == doctest::Approx(0.f));

  CHECK(value.state_->tick(101.50));
  CHECK(value.get() == doctest::Approx(5.f));

  CHECK_FALSE(value.state_->tick(103.00));
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

  value.state_->startTime = 10.0;

  CHECK(value.state_->tick(10.50));
  CHECK(value.get() == doctest::Approx(5.f));

  CHECK(value.state_->tick(11.50));
  CHECK(value.get() == doctest::Approx(5.f));

  CHECK_FALSE(value.state_->tick(12.00));
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

TEST_CASE("Animation copies share playback state") {
  Animation<float> original{0.f};
  Animation<float> copy = original;

  copy.play(10.f, Transition::linear(1.f));

  CHECK(original.isRunning());
  CHECK(copy.isRunning());

  original.state_->startTime = 20.0;
  REQUIRE(copy.state_->tick(20.5));
  CHECK(original.get() == doctest::Approx(5.f));

  original.stop();
  CHECK_FALSE(copy.isRunning());
}

TEST_CASE("Toggle state changes drive thumb through animation instead of jumping immediately") {
  struct Root {
    Signal<bool> value;

    Element body() const {
      return Toggle{.value = value};
    }
  };

  Signal<bool> value{false};
  FakeTextSystem textSystem;
  scenegraph::SceneGraph sceneGraph;
  MountRoot root{
      std::make_unique<TypedRootHolder<Root>>(std::in_place, Root{value}),
      textSystem,
      testEnvironment(),
      Size{120.f, 80.f},
  };

  root.mount(sceneGraph);

  scenegraph::RectNode const* thumb = findMovedThumb(sceneGraph.root());
  REQUIRE(thumb != nullptr);
  float const initialX = thumb->position().x;
  CHECK(initialX == doctest::Approx(4.f));

  value = true;

  thumb = findMovedThumb(sceneGraph.root());
  REQUIRE(thumb != nullptr);
  CHECK(thumb->position().x == doctest::Approx(initialX));
}
