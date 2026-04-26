#include <doctest/doctest.h>

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive2/Bindable.hpp>
#include <Flux/Reactive2/Scope.hpp>
#include <Flux/Reactive2/Signal.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

#include <chrono>
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

flux::EnvironmentLayer themeEnvironment(flux::Reactive2::Signal<flux::Theme> theme) {
  flux::EnvironmentLayer environment;
  environment.setSignal(std::move(theme));
  return environment;
}

flux::Color solidColor(flux::scenegraph::RectNode const& rect) {
  flux::Color color{};
  CHECK(rect.fill().solidColor(&color));
  return color;
}

void checkSameChannels(flux::Color actual, flux::Color expected) {
  CHECK(actual.r == doctest::Approx(expected.r));
  CHECK(actual.g == doctest::Approx(expected.g));
  CHECK(actual.b == doctest::Approx(expected.b));
  CHECK(actual.a == doctest::Approx(expected.a));
}

} // namespace

TEST_CASE("theme signal updates retained leaf bindings without remounting") {
  struct Root {
    int* bodyCalls = nullptr;
    int* cleanups = nullptr;

    flux::Element body() const {
      ++*bodyCalls;
      auto theme = flux::useEnvironment<flux::Theme>();
      flux::Reactive2::onCleanup([cleanups = cleanups] {
        ++*cleanups;
      });

      return flux::Element{flux::Rectangle{}}
          .size(32.f, 18.f)
          .fill(flux::Reactive2::Bindable<flux::Color>{[theme] {
            return flux::Color::windowBackground();
          }})
          .stroke(flux::Reactive2::Bindable<flux::Color>{[theme] {
            return theme().separatorColor;
          }}, flux::Reactive2::Bindable<float>{1.f});
    }
  };

  int bodyCalls = 0;
  int cleanups = 0;
  flux::Reactive2::Signal<flux::Theme> theme{flux::Theme::light()};
  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{&bodyCalls, &cleanups}),
      textSystem,
      themeEnvironment(theme),
      flux::Size{200.f, 120.f},
  };

  root.mount(sceneGraph);

  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Rect);
  auto const* initialNode = &static_cast<flux::scenegraph::RectNode const&>(sceneGraph.root());
  CHECK(bodyCalls == 1);
  CHECK(cleanups == 0);
  checkSameChannels(solidColor(*initialNode), flux::Theme::light().windowBackgroundColor);

  auto const toggleStart = std::chrono::steady_clock::now();
  theme.set(flux::Theme::dark());
  auto const toggleElapsed = std::chrono::steady_clock::now() - toggleStart;
  CHECK(std::chrono::duration<float, std::milli>(toggleElapsed).count() < 16.67f);

  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Rect);
  auto const* updatedNode = &static_cast<flux::scenegraph::RectNode const&>(sceneGraph.root());
  CHECK(updatedNode == initialNode);
  CHECK(bodyCalls == 1);
  CHECK(cleanups == 0);
  checkSameChannels(solidColor(*updatedNode), flux::Theme::dark().windowBackgroundColor);
  CHECK(updatedNode->stroke().color == flux::Theme::dark().separatorColor);

  root.unmount(sceneGraph);
  CHECK(cleanups == 1);
}

TEST_CASE("themeField exposes a computed theme member to bindable leaves") {
  struct Root {
    int* bodyCalls = nullptr;

    flux::Element body() const {
      ++*bodyCalls;
      auto space3 = flux::themeField(&flux::Theme::space3);
      return flux::Element{flux::Rectangle{}}
          .size(flux::Reactive2::Bindable<float>{[space3] {
                  return space3.get();
                }},
                flux::Reactive2::Bindable<float>{10.f})
          .fill(flux::Colors::blue);
    }
  };

  int bodyCalls = 0;
  flux::Reactive2::Signal<flux::Theme> theme{flux::Theme::light()};
  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{&bodyCalls}),
      textSystem,
      themeEnvironment(theme),
      flux::Size{200.f, 120.f},
  };

  root.mount(sceneGraph);

  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Rect);
  auto const& rect = static_cast<flux::scenegraph::RectNode const&>(sceneGraph.root());
  CHECK(rect.size().width == doctest::Approx(flux::Theme::light().space3));
  CHECK(bodyCalls == 1);

  theme.set(flux::Theme::comfortable());

  CHECK(rect.size().width == doctest::Approx(flux::Theme::comfortable().space3));
  CHECK(bodyCalls == 1);
}
