#include <doctest/doctest.h>

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/MountContext.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextInput.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

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

class MeasuringTextSystem final : public flux::TextSystem {
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
    return {80.f, 16.f};
  }

  flux::Size measure(std::string_view text, flux::Font const&, flux::Color const&, float,
                     flux::TextLayoutOptions const&) override {
    return {std::max(24.f, static_cast<float>(text.size()) * 6.f), 16.f};
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

struct IntrinsicBox {
  flux::Size measure(flux::MeasureContext& ctx, flux::LayoutConstraints const&,
                     flux::LayoutHints const&, flux::TextSystem&) const {
    ctx.advanceChildSlot();
    return {24.f, 12.f};
  }

  std::unique_ptr<flux::scenegraph::SceneNode> mount(flux::MountContext&) const {
    return std::make_unique<flux::scenegraph::RectNode>(
        flux::Rect{0.f, 0.f, 24.f, 12.f});
  }
};

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

TEST_CASE("modifier envelopes honor fixed viewport constraints") {
  struct Root {
    flux::Element body() const {
      return flux::Element{IntrinsicBox{}}
          .onTap([] {});
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{}),
      textSystem,
      testEnvironment(),
      flux::Size{200.f, 100.f},
  };

  root.mount(sceneGraph);

  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Rect);
  CHECK(sceneGraph.root().size() == flux::Size{200.f, 100.f});
  REQUIRE(sceneGraph.root().children().size() == 1);
  CHECK(sceneGraph.root().children()[0]->size() == flux::Size{24.f, 12.f});
}

TEST_CASE("MountContext child creates a scoped owner") {
  flux::Reactive::Scope rootScope;
  flux::EnvironmentStack environment;
  FakeTextSystem textSystem;
  flux::MeasureContext measureContext{textSystem};
  flux::MountContext rootContext{
      rootScope,
      environment,
      textSystem,
      measureContext,
      flux::LayoutConstraints{.maxWidth = 100.f, .maxHeight = 100.f},
  };

  int childCleanups = 0;
  {
    flux::MountContext childContext =
        rootContext.child(flux::LayoutConstraints{.maxWidth = 40.f, .maxHeight = 20.f});
    CHECK(&childContext.owner() != &rootContext.owner());
    childContext.owner().onCleanup([&childCleanups] {
      ++childCleanups;
    });
  }

  CHECK(childCleanups == 0);
  rootScope.dispose();
  CHECK(childCleanups == 1);
}

TEST_CASE("MountRoot resize relayouts without remounting root state") {
  int bodyCalls = 0;

  struct Root {
    int* bodyCalls = nullptr;

    flux::Element body() const {
      ++*bodyCalls;
      auto width = flux::useState(20.f);
      return flux::Element{flux::Rectangle{}}
          .width(flux::Reactive::Bindable<float>{[width] {
            return width.get();
          }})
          .height(10.f)
          .onTap([width] {
            width.set(64.f);
          });
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
  REQUIRE(sceneGraph.root().interaction() != nullptr);
  REQUIRE(sceneGraph.root().interaction()->onTap);
  sceneGraph.root().interaction()->onTap();
  CHECK(sceneGraph.root().size() == flux::Size{64.f, 10.f});

  root.resize(flux::Size{320.f, 180.f}, sceneGraph);

  CHECK(bodyCalls == 1);
  CHECK(sceneGraph.root().size() == flux::Size{64.f, 10.f});
}

TEST_CASE("MountRoot resize updates viewport-sized root without remount") {
  int bodyCalls = 0;

  struct Root {
    int* bodyCalls = nullptr;

    flux::Element body() const {
      ++*bodyCalls;
      return flux::Rectangle{};
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
  CHECK(sceneGraph.root().size() == flux::Size{200.f, 100.f});

  root.resize(flux::Size{320.f, 180.f}, sceneGraph);

  CHECK(bodyCalls == 1);
  CHECK(sceneGraph.root().size() == flux::Size{320.f, 180.f});
}

TEST_CASE("MountRoot resize preserves direct text positions in stacks") {
  struct Root {
    flux::Element body() const {
      return flux::ScrollView{
          .axis = flux::ScrollAxis::Vertical,
          .children = flux::children(
              flux::Element{flux::VStack{
                  .spacing = 16.f,
                  .alignment = flux::Alignment::Stretch,
                  .children = flux::children(
                      flux::Text{.text = "Alert demo", .font = flux::Font::largeTitle()},
                      flux::Text{.text = "Modal alerts via useAlert().",
                                 .font = flux::Font::body(),
                                 .wrapping = flux::TextWrapping::Wrap},
                      flux::Text{.text = "Tap a button to open an alert.",
                                 .font = flux::Font::footnote(),
                                 .wrapping = flux::TextWrapping::Wrap}),
              }}.padding(24.f)),
      };
    }
  };

  MeasuringTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{}),
      textSystem,
      testEnvironment(),
      flux::Size{800.f, 800.f},
  };

  root.mount(sceneGraph);

  auto const& viewport = static_cast<flux::scenegraph::RectNode const&>(sceneGraph.root());
  auto const& content = static_cast<flux::scenegraph::GroupNode const&>(*viewport.children()[0]);
  auto const& paddedStack = static_cast<flux::scenegraph::RectNode const&>(*content.children()[0]);
  auto const& stack = static_cast<flux::scenegraph::GroupNode const&>(*paddedStack.children()[0]);
  REQUIRE(stack.children().size() == 3);
  float const firstY = stack.children()[0]->position().y;
  float const secondY = stack.children()[1]->position().y;
  float const thirdY = stack.children()[2]->position().y;
  CHECK(firstY == doctest::Approx(0.f));
  CHECK(secondY > firstY);
  CHECK(thirdY > secondY);

  root.resize(flux::Size{900.f, 700.f}, sceneGraph);

  CHECK(stack.children()[0]->position().y == doctest::Approx(firstY));
  CHECK(stack.children()[1]->position().y == doctest::Approx(secondY));
  CHECK(stack.children()[2]->position().y == doctest::Approx(thirdY));
}

TEST_CASE("ScaleAroundCenter relayout keeps reactive scale binding alive") {
  struct Root {
    flux::Reactive::Signal<float> scale;

    flux::Element body() const {
      return flux::ScaleAroundCenter{
          .scale = flux::Reactive::Bindable<float>{[scale = scale] {
            return scale.get();
          }},
          .child = flux::Element{flux::Rectangle{}}
                       .size(20.f, 10.f)
                       .fill(flux::Colors::red),
      };
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::Reactive::Signal<float> scale{0.96f};
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{scale}),
      textSystem,
      testEnvironment(),
      flux::Size{200.f, 100.f},
  };

  root.mount(sceneGraph);
  scale.set(0.92f);

  root.resize(flux::Size{320.f, 180.f}, sceneGraph);

  CHECK(sceneGraph.root().size().width >= 20.f);
  CHECK(sceneGraph.root().size().height >= 10.f);
}

TEST_CASE("TextInput fills finite assigned stack width") {
  struct Root {
    flux::Element body() const {
      auto value = flux::useState(std::string{"hello"});
      return flux::VStack{
          .alignment = flux::Alignment::Start,
          .children = flux::children(flux::TextInput{
              .value = value,
              .multiline = true,
              .multilineHeight = {.fixed = 40.f},
          }),
      };
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{}),
      textSystem,
      testEnvironment(),
      flux::Size{180.f, 100.f},
  };

  root.mount(sceneGraph);

  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Group);
  REQUIRE(sceneGraph.root().children().size() == 1);
  CHECK(sceneGraph.root().children()[0]->size().width == doctest::Approx(180.f));
}

TEST_CASE("ScrollView mount emits overlay indicators for overflowing content") {
  struct Root {
    flux::Element body() const {
      return flux::ScrollView{
          .axis = flux::ScrollAxis::Vertical,
          .children = flux::children(
              flux::Rectangle{}.size(60.f, 30.f),
              flux::Rectangle{}.size(60.f, 30.f)),
      };
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{}),
      textSystem,
      testEnvironment(),
      flux::Size{80.f, 40.f},
  };

  root.mount(sceneGraph);

  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Rect);
  auto const& viewport = static_cast<flux::scenegraph::RectNode const&>(sceneGraph.root());
  CHECK(viewport.clipsContents());
  REQUIRE(viewport.children().size() == 2);
  REQUIRE(viewport.children()[0]->kind() == flux::scenegraph::SceneNodeKind::Group);
  REQUIRE(viewport.children()[1]->kind() == flux::scenegraph::SceneNodeKind::Rect);

  auto const& overlay = static_cast<flux::scenegraph::RectNode const&>(*viewport.children()[1]);
  CHECK(overlay.opacity() == doctest::Approx(0.f));
  REQUIRE(overlay.children().size() == 1);
  CHECK(overlay.children()[0]->bounds().x == doctest::Approx(73.f));

  REQUIRE(viewport.interaction() != nullptr);
  REQUIRE(viewport.interaction()->onScroll);
  viewport.interaction()->onScroll(flux::Vec2{0.f, -12.f});

  CHECK(viewport.children()[0]->position().y == doctest::Approx(-12.f));
  CHECK(overlay.opacity() == doctest::Approx(1.f));
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

TEST_CASE("nested body component bindings inherit the root redraw callback") {
  struct Child {
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

  struct Root {
    flux::Reactive::Signal<bool> hot;

    flux::Element body() const {
      return flux::Element{Child{hot}};
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::Reactive::Signal<bool> hot{true};
  int redraws = 0;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{hot}),
      textSystem,
      testEnvironment(),
      flux::Size{200.f, 100.f},
      [&] { ++redraws; },
  };

  root.mount(sceneGraph);
  redraws = 0;
  hot.set(false);

  CHECK(redraws == 1);
}

TEST_CASE("container mounting composes slot origin with explicit child position") {
  struct Root {
    flux::Element body() const {
      return flux::ZStack{
          .horizontalAlignment = flux::Alignment::Start,
          .verticalAlignment = flux::Alignment::Start,
          .children = flux::children(
              flux::Rectangle{}
                  .size(44.f, 26.f)
                  .fill(flux::Colors::blue),
              flux::Rectangle{}
                  .size(18.f, 18.f)
                  .position(22.f, 4.f)
                  .fill(flux::Colors::red)),
      };
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{}),
      textSystem,
      testEnvironment(),
      flux::Size{44.f, 26.f},
  };

  root.mount(sceneGraph);

  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Group);
  auto const& group = static_cast<flux::scenegraph::GroupNode const&>(sceneGraph.root());
  REQUIRE(group.children().size() == 2);
  CHECK(group.children()[0]->position() == flux::Point{0.f, 0.f});
  CHECK(group.children()[1]->position() == flux::Point{22.f, 4.f});
}
