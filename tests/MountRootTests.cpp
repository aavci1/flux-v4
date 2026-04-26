#include <doctest/doctest.h>

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

#include <memory>
#include <string_view>
#include <vector>

namespace {

class FakeTextSystem final : public flux::TextSystem {
public:
  std::shared_ptr<flux::TextLayout const>
  layout(flux::AttributedString const&, float, flux::TextLayoutOptions const&) override {
    return std::make_shared<flux::TextLayout>();
  }

  std::shared_ptr<flux::TextLayout const>
  layout(std::string_view, flux::Font const&, flux::Color const&, float,
         flux::TextLayoutOptions const&) override {
    return std::make_shared<flux::TextLayout>();
  }

  flux::Size measure(flux::AttributedString const&, float,
                     flux::TextLayoutOptions const&) override {
    return {0.f, 0.f};
  }

  flux::Size measure(std::string_view, flux::Font const&, flux::Color const&, float,
                     flux::TextLayoutOptions const&) override {
    return {0.f, 0.f};
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float,
                                           std::uint32_t& outWidth,
                                           std::uint32_t& outHeight,
                                           flux::Point& outBearing) override {
    outWidth = 0;
    outHeight = 0;
    outBearing = {};
    return {};
  }
};

flux::EnvironmentLayer testEnvironment() {
  flux::EnvironmentLayer environment;
  environment.set(flux::Theme::light());
  return environment;
}

flux::Color solidColor(flux::scenegraph::RectNode const& rect) {
  flux::Color color{};
  CHECK(rect.fill().solidColor(&color));
  return color;
}

} // namespace

TEST_CASE("MountRoot mounts a static root once") {
  int bodyCalls = 0;

  struct Root {
    int* bodyCalls = nullptr;

    flux::Element body() const {
      ++*bodyCalls;
      return flux::Element{flux::Rectangle{}}
          .size(20.f, 30.f)
          .fill(flux::Colors::red);
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{&bodyCalls}),
      textSystem,
      testEnvironment(),
      flux::Size{200.f, 100.f},
  };

  root.mount(sceneGraph);

  CHECK(root.mounted());
  CHECK(bodyCalls == 1);
  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Rect);
  auto const& rect = static_cast<flux::scenegraph::RectNode const&>(sceneGraph.root());
  CHECK(rect.size() == flux::Size{20.f, 30.f});
  CHECK(solidColor(rect) == flux::Colors::red);
}

TEST_CASE("MountRoot keeps Bindable effects scoped to the mount") {
  struct Root {
    flux::Reactive::Signal<bool> hot;

    flux::Element body() const {
      return flux::Element{flux::Rectangle{}}
          .size(10.f, 10.f)
          .fill(flux::Reactive::Bindable<flux::Color>{
              [hot = hot] {
                return hot() ? flux::Colors::red : flux::Colors::blue;
              }});
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::Reactive::Signal<bool> hot{true};
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{hot}),
      textSystem,
      testEnvironment(),
      flux::Size{200.f, 100.f},
  };

  root.mount(sceneGraph);

  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Rect);
  auto const& rect = static_cast<flux::scenegraph::RectNode const&>(sceneGraph.root());
  CHECK(solidColor(rect) == flux::Colors::red);

  hot.set(false);
  CHECK(solidColor(rect) == flux::Colors::blue);

  root.unmount(sceneGraph);
  CHECK_FALSE(root.mounted());
  CHECK(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Group);
  hot.set(true);
}
