#include <doctest/doctest.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Scene/CustomTransformSceneNode.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Scene/PathSceneNode.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/Renderer.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/Scene/SceneTreeInteraction.hpp>
#include <Flux/Scene/TextSceneNode.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/FocusController.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/GestureTracker.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/SceneGeometryIndex.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <memory>
#include <chrono>
#include <string_view>
#include <thread>
#include <typeindex>

namespace {

using namespace flux;

class NullTextSystem : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const&, float, TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    out->measuredSize = Size{48.f, 14.f};
    return out;
  }

  std::shared_ptr<TextLayout const> layout(std::string_view, Font const&, Color const&, float,
                                           TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    out->measuredSize = Size{48.f, 14.f};
    return out;
  }

  Size measure(AttributedString const& text, float, TextLayoutOptions const&) override {
    return Size{std::max(1.f, 8.f * static_cast<float>(text.utf8.size())), 14.f};
  }

  Size measure(std::string_view, Font const&, Color const&, float, TextLayoutOptions const&) override {
    return Size{48.f, 14.f};
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }
};

class CountingTextSystem final : public NullTextSystem {
public:
  int layoutCount = 0;

  std::shared_ptr<TextLayout const> layout(AttributedString const& text, float maxWidth,
                                           TextLayoutOptions const& options) override {
    ++layoutCount;
    return NullTextSystem::layout(text, maxWidth, options);
  }

  std::shared_ptr<TextLayout const> layout(std::string_view text, Font const& font, Color const& color,
                                           float maxWidth, TextLayoutOptions const& options) override {
    ++layoutCount;
    return NullTextSystem::layout(text, font, color, maxWidth, options);
  }
};

class VariableTextSystem final : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const& text, float maxWidth,
                                           TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    out->measuredSize = measuredSizeForLength(text.utf8.size(), maxWidth);
    return out;
  }

  std::shared_ptr<TextLayout const> layout(std::string_view text, Font const&, Color const&, float maxWidth,
                                           TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    out->measuredSize = measuredSizeForLength(text.size(), maxWidth);
    return out;
  }

  Size measure(AttributedString const& text, float maxWidth, TextLayoutOptions const&) override {
    return measuredSizeForLength(text.utf8.size(), maxWidth);
  }

  Size measure(std::string_view text, Font const&, Color const&, float maxWidth,
               TextLayoutOptions const&) override {
    return measuredSizeForLength(text.size(), maxWidth);
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }

private:
  static Size measuredSizeForLength(std::size_t length, float maxWidth) {
    float const intrinsicWidth = std::max(1.f, 7.f * static_cast<float>(length));
    if (maxWidth > 0.f) {
      float const lineWidth = std::min(intrinsicWidth, maxWidth);
      float const lines = std::max(1.f, std::ceil(intrinsicWidth / maxWidth));
      return Size{lineWidth, 14.f * lines};
    }
    return Size{intrinsicWidth, 14.f};
  }
};

class NullRenderer final : public Renderer {
public:
  int rectCount = 0;
  int textCount = 0;
  int pathCount = 0;
  std::vector<Rect> rects;
  std::vector<Point> textOrigins;

  void save() override {}
  void restore() override {}
  void translate(Point) override {}
  void transform(Mat3 const&) override {}
  void clipRect(Rect, bool = false) override {}
  bool quickReject(Rect) const override { return false; }
  void setOpacity(float) override {}
  void setBlendMode(BlendMode) override {}
  void drawRect(Rect const& rect, CornerRadius const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override {
    ++rectCount;
    rects.push_back(rect);
  }
  void drawLine(Point, Point, StrokeStyle const&) override {}
  void drawPath(Path const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override { ++pathCount; }
  void drawTextLayout(TextLayout const&, Point origin) override {
    ++textCount;
    textOrigins.push_back(origin);
  }
  void drawImage(Image const&, Rect const&, ImageFillMode, CornerRadius const&, float) override {}
};

struct EnvironmentScope {
  explicit EnvironmentScope(EnvironmentLayer layer) {
    EnvironmentStack::current().push(std::move(layer));
  }
  ~EnvironmentScope() { EnvironmentStack::current().pop(); }
};

struct StoreScope {
  flux::StateStore store;
  flux::StateStore* previous = nullptr;

  StoreScope() {
    previous = flux::StateStore::current();
    flux::StateStore::setCurrent(&store);
  }

  ~StoreScope() { flux::StateStore::setCurrent(previous); }
};

struct ScrollSizedComposite {
  std::string key;
  float width = 0.f;
  float height = 0.f;

  Element body() const {
    return Element{Rectangle{}}
        .key(key)
        .size(width, height);
  }
};

} // namespace

namespace {

Element keyedRect(std::string key, float width, float height) {
  return Element{Rectangle{}}.key(std::move(key)).size(width, height);
}

Element demoColorBlock(float width, float height) {
  return Element{Rectangle{}}.size(width, height);
}

Element demoSectionCard(std::string title, std::string caption, Element content) {
  return VStack{
      .spacing = 16.f,
      .children = children(
          Text{
              .text = std::move(title),
              .horizontalAlignment = HorizontalAlignment::Leading,
          },
          Text{
              .text = std::move(caption),
              .horizontalAlignment = HorizontalAlignment::Leading,
              .wrapping = TextWrapping::Wrap,
          },
          std::move(content)
      ),
  }
      .padding(16.f)
      .fill(FillStyle::solid(Color::hex(0xFFFFFF)))
      .stroke(StrokeStyle::solid(Color::hex(0xE0E0E0), 1.f))
      .cornerRadius(CornerRadius{12.f});
}

Element demoVStackCard() {
  return demoSectionCard(
      "VStack",
      "Children flow top-to-bottom. Center alignment keeps each child at its intrinsic width and centers it in the column.",
      VStack{
          .spacing = 12.f,
          .alignment = Alignment::Center,
          .children = children(
              demoColorBlock(160.f, 34.f),
              demoColorBlock(220.f, 42.f),
              demoColorBlock(120.f, 30.f),
              HStack{
                  .spacing = 8.f,
                  .alignment = Alignment::Center,
                  .children = children(
                      Text{
                          .text = "Rows can also host nested stacks",
                          .horizontalAlignment = HorizontalAlignment::Leading,
                      },
                      Spacer{},
                      Text{
                          .text = "nested",
                          .horizontalAlignment = HorizontalAlignment::Trailing,
                      }
                  )
              }
                  .padding(12.f)
                  .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
                  .cornerRadius(CornerRadius{8.f})
          )
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoHStackCard() {
  return demoSectionCard(
      "HStack",
      "Children flow left-to-right. Flex growth lets selected items absorb remaining width.",
      VStack{
          .spacing = 12.f,
          .children = children(
              HStack{
                  .spacing = 12.f,
                  .alignment = Alignment::Stretch,
                  .children = children(
                      demoColorBlock(56.f, 54.f).flex(2.f, 1.f, 0.f),
                      demoColorBlock(56.f, 76.f),
                      demoColorBlock(56.f, 40.f).flex(1.f, 1.f, 0.f),
                      demoColorBlock(56.f, 54.f)
                  )
              },
              HStack{
                  .spacing = 0.f,
                  .alignment = Alignment::Center,
                  .children = children(
                      Text{.text = "Leading"},
                      Spacer{},
                      Text{
                          .text = "Spacer pushes this trailing label",
                          .horizontalAlignment = HorizontalAlignment::Trailing,
                      }
                  )
              }
                  .padding(12.f)
                  .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
                  .cornerRadius(CornerRadius{8.f})
          )
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoZStackCard() {
  return demoSectionCard(
      "ZStack",
      "Children share the same space. This is useful for overlays, badges, and stacked decoration.",
      ZStack{
          .horizontalAlignment = Alignment::Center,
          .verticalAlignment = Alignment::Center,
          .children = children(
              Element{Rectangle{}}.size(0.f, 180.f),
              Element{Rectangle{}}.size(220.f, 104.f),
              Element{VStack{
                  .spacing = 4.f,
                  .alignment = Alignment::Center,
                  .children = children(
                      Text{
                          .text = "Overlay content",
                          .horizontalAlignment = HorizontalAlignment::Center,
                      },
                      Text{
                          .text = "Centered inside a shared layer",
                          .horizontalAlignment = HorizontalAlignment::Center,
                      }
                  )
              }}
          )
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoLayoutRoot() {
  return ScrollView{
      .axis = ScrollAxis::Vertical,
      .children = children(
          VStack{
              .spacing = 16.f,
              .children = children(
                  Text{
                      .text = "Layout Demo",
                      .horizontalAlignment = HorizontalAlignment::Leading,
                  },
                  Text{
                      .text =
                          "Focused examples for VStack, HStack, ZStack, Grid, and how they compose in practice.",
                      .horizontalAlignment = HorizontalAlignment::Leading,
                      .wrapping = TextWrapping::Wrap,
                  },
                  demoVStackCard(),
                  demoHStackCard(),
                  demoZStackCard()
              )
          }
              .padding(20.f)
      )
  }
      .fill(FillStyle::solid(Color::hex(0xFFFFFF)));
}

struct InteractiveRectTree {
  SceneTree tree;
  NodeId leafId{};
};

InteractiveRectTree makeInteractiveRectTree(std::string key, bool focusable = false,
                                            std::function<void()> onTap = {}) {
  auto root = std::make_unique<SceneNode>(NodeId{1ull});
  auto rect = std::make_unique<RectSceneNode>(NodeId{2ull});
  RectSceneNode* rectPtr = rect.get();
  rect->size = Size{40.f, 20.f};
  auto interaction = std::make_unique<InteractionData>();
  interaction->stableTargetKey = ComponentKey{LocalId::fromString(key)};
  interaction->focusable = focusable;
  interaction->onTap = std::move(onTap);
  rect->setInteraction(std::move(interaction));
  rect->recomputeBounds();
  root->appendChild(std::move(rect));
  root->recomputeBounds();
  return InteractiveRectTree{SceneTree{std::move(root)}, rectPtr->id()};
}

struct HelloRoot {
  Element body() const {
    return Text{
        .text = "Hello, World!",
    };
  }
};

struct RetainedTextChild {
  int* bodyCalls = nullptr;

  Element body() const {
    ++*bodyCalls;
    return Text{
        .text = "Retained child",
    };
  }
};

struct RetainedParent {
  State<int> tick{};
  int* parentCalls = nullptr;
  int* childCalls = nullptr;

  Element body() const {
    ++*parentCalls;
    return VStack{
        .spacing = static_cast<float>(*tick),
        .children = {
            Element{Rectangle{}}.size(24.f + static_cast<float>(*tick), 8.f),
            Element{RetainedTextChild{childCalls}}.key("child"),
        },
    };
  }
};

struct CompositeRootScrollView {
  Element body() const {
    auto hostState = useState(0);
    (void)hostState;
    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = {
            keyedRect("a", 60.f, 30.f),
            keyedRect("b", 60.f, 30.f),
        },
    };
  }
};

} // namespace

TEST_CASE("SceneBuilder: keyed reorder reuses child scene nodes") {
  NullTextSystem textSystem{};
  SceneGeometryIndex geometry{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &geometry};
  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 200.f;

  Element first = VStack{
      .spacing = 4.f,
      .children = {
          keyedRect("a", 20.f, 10.f),
          keyedRect("b", 20.f, 10.f),
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(first, NodeId{1ull}, constraints);
  REQUIRE(tree);
  REQUIRE(tree->children().size() == 2);

  SceneNode* firstA = tree->children()[0].get();
  SceneNode* firstB = tree->children()[1].get();

  Element reordered = VStack{
      .spacing = 4.f,
      .children = {
          keyedRect("b", 20.f, 10.f),
          keyedRect("a", 20.f, 10.f),
      },
  };

  tree = builder.build(reordered, NodeId{1ull}, constraints, std::move(tree));
  REQUIRE(tree);
  REQUIRE(tree->children().size() == 2);
  CHECK(tree->children()[0].get() == firstB);
  CHECK(tree->children()[1].get() == firstA);
}

TEST_CASE("SceneBuilder: scroll rebuild reuses descendants without repaint dirties") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 80.f;
  constraints.maxHeight = 40.f;

  Signal<Point> scrollSignal{Point{0.f, 0.f}};
  State<Point> scroll{&scrollSignal};

  auto makeScrollElement = [&]() -> Element {
    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .scrollOffset = scroll,
        .children = {
            keyedRect("a", 60.f, 30.f),
            keyedRect("b", 60.f, 30.f),
        },
    };
  };

  std::unique_ptr<SceneNode> tree = builder.build(makeScrollElement(), NodeId{1ull}, constraints);
  REQUIRE(tree);
  auto* scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  REQUIRE(scrollRoot->children().size() == 1);
  SceneNode* viewportGroup = scrollRoot->children()[0].get();
  REQUIRE(viewportGroup->children().size() >= 1);
  SceneNode* contentGroup = viewportGroup->children()[0].get();
  REQUIRE(contentGroup->children().size() == 2);
  SceneNode* firstA = contentGroup->children()[0].get();
  SceneNode* firstB = contentGroup->children()[1].get();

  NullRenderer renderer{};
  render(*tree, renderer);
  CHECK_FALSE(firstA->paintDirty());
  CHECK_FALSE(firstB->paintDirty());

  scroll = Point{0.f, 12.f};
  tree = builder.build(makeScrollElement(), NodeId{1ull}, constraints, std::move(tree));
  REQUIRE(tree);
  scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  REQUIRE(scrollRoot->children().size() == 1);
  viewportGroup = scrollRoot->children()[0].get();
  REQUIRE(viewportGroup->children().size() >= 1);
  contentGroup = viewportGroup->children()[0].get();
  REQUIRE(contentGroup->children().size() == 2);

  CHECK(contentGroup->children()[0].get() == firstA);
  CHECK(contentGroup->children()[1].get() == firstB);
  CHECK(contentGroup->children()[0]->position.y == doctest::Approx(-12.f));
  CHECK(contentGroup->children()[1]->position.y == doctest::Approx(18.f));
  CHECK_FALSE(firstA->paintDirty());
  CHECK_FALSE(firstB->paintDirty());
}

TEST_CASE("SceneBuilder: scroll view local wheel state moves content and shows indicators") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 80.f;
  constraints.maxHeight = 40.f;

  auto makeScrollElement = [&]() -> Element {
    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = {
            keyedRect("a", 60.f, 30.f),
            keyedRect("b", 60.f, 30.f),
        },
    };
  };

  std::unique_ptr<SceneNode> tree = builder.build(makeScrollElement(), NodeId{1ull}, constraints);
  REQUIRE(tree);
  auto* scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  REQUIRE(scrollRoot->interaction() != nullptr);
  REQUIRE(scrollRoot->interaction()->onScroll);
  REQUIRE(scrollRoot->children().size() == 1);

  SceneNode* viewportGroup = scrollRoot->children()[0].get();
  REQUIRE(viewportGroup->children().size() == 2);
  auto* indicatorOverlay = dynamic_cast<ModifierSceneNode*>(viewportGroup->children()[1].get());
  REQUIRE(indicatorOverlay != nullptr);
  CHECK(indicatorOverlay->opacity == doctest::Approx(0.f));

  SceneNode* contentGroup = viewportGroup->children()[0].get();
  REQUIRE(contentGroup->children().size() == 2);
  CHECK(contentGroup->children()[0]->position.y == doctest::Approx(0.f));
  CHECK(contentGroup->children()[1]->position.y == doctest::Approx(30.f));

  scrollRoot->interaction()->onScroll(Vec2{0.f, -12.f});
  tree = builder.build(makeScrollElement(), NodeId{1ull}, constraints, std::move(tree));
  REQUIRE(tree);
  scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  viewportGroup = scrollRoot->children()[0].get();
  REQUIRE(viewportGroup->children().size() == 2);
  indicatorOverlay = dynamic_cast<ModifierSceneNode*>(viewportGroup->children()[1].get());
  REQUIRE(indicatorOverlay != nullptr);
  CHECK(indicatorOverlay->opacity == doctest::Approx(1.f));
  contentGroup = viewportGroup->children()[0].get();
  REQUIRE(contentGroup->children().size() == 2);

  CHECK(contentGroup->children()[0]->position.y == doctest::Approx(-12.f));
  CHECK(contentGroup->children()[1]->position.y == doctest::Approx(18.f));
}

TEST_CASE("SceneBuilder: scroll indicators fade out after inactivity") {
  using namespace std::chrono_literals;

  Application app;
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 80.f;
  constraints.maxHeight = 40.f;

  auto makeScrollElement = [&]() -> Element {
    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = {
            keyedRect("a", 60.f, 30.f),
            keyedRect("b", 60.f, 30.f),
        },
    };
  };

  std::unique_ptr<SceneNode> tree = builder.build(makeScrollElement(), NodeId{1ull}, constraints);
  REQUIRE(tree);
  auto* scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  REQUIRE(scrollRoot->interaction() != nullptr);
  REQUIRE(scrollRoot->interaction()->onScroll);

  scrollRoot->interaction()->onScroll(Vec2{0.f, -12.f});
  tree = builder.build(makeScrollElement(), NodeId{1ull}, constraints, std::move(tree));
  REQUIRE(tree);
  scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  auto* viewportGroup = scrollRoot->children()[0].get();
  REQUIRE(viewportGroup->children().size() == 2);
  auto* indicatorOverlay = dynamic_cast<ModifierSceneNode*>(viewportGroup->children()[1].get());
  REQUIRE(indicatorOverlay != nullptr);
  CHECK(indicatorOverlay->opacity == doctest::Approx(1.f));

  std::jthread stopLater([&app] {
    std::this_thread::sleep_for(1200ms);
    app.quit();
  });
  CHECK(app.exec() == 0);

  tree = builder.build(makeScrollElement(), NodeId{1ull}, constraints, std::move(tree));
  REQUIRE(tree);
  scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  viewportGroup = scrollRoot->children()[0].get();
  REQUIRE(viewportGroup->children().size() == 2);
  indicatorOverlay = dynamic_cast<ModifierSceneNode*>(viewportGroup->children()[1].get());
  REQUIRE(indicatorOverlay != nullptr);
  CHECK(indicatorOverlay->opacity == doctest::Approx(0.f).epsilon(0.05f));
}

TEST_CASE("SceneBuilder: composite root scroll view keeps local state separate from outer body") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  scope.store.beginRebuild();
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 80.f;
  constraints.maxHeight = 40.f;

  Element root = CompositeRootScrollView{};
  std::unique_ptr<SceneNode> tree = builder.build(root, NodeId{1ull}, constraints);
  REQUIRE(tree);
  auto* scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  REQUIRE(scrollRoot->interaction() != nullptr);
  REQUIRE(scrollRoot->interaction()->onScroll);

  scrollRoot->interaction()->onScroll(Vec2{0.f, -12.f});
  tree = builder.build(root, NodeId{1ull}, constraints, std::move(tree));
  REQUIRE(tree);
  scrollRoot = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(scrollRoot != nullptr);
  SceneNode* viewportGroup = scrollRoot->children()[0].get();
  SceneNode* contentGroup = viewportGroup->children()[0].get();
  REQUIRE(contentGroup->children().size() == 2);
  CHECK(contentGroup->children()[0]->position.y == doctest::Approx(-12.f));
  CHECK(contentGroup->children()[1]->position.y == doctest::Approx(18.f));
}

TEST_CASE("SceneBuilder: scroll view descendants keep the same composite identities between measure and build") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 320.f;
  constraints.maxHeight = 180.f;

  Element root = ScrollView{
      .axis = ScrollAxis::Vertical,
      .children = children(
          ScrollSizedComposite{.key = "first", .width = 40.f, .height = 180.f},
          ScrollSizedComposite{.key = "second", .width = 50.f, .height = 220.f},
          ScrollSizedComposite{.key = "third", .width = 60.f, .height = 120.f}),
  };

  scope.store.beginRebuild(true);
  std::unique_ptr<SceneNode> tree = builder.build(root, NodeId{1ull}, constraints);
  scope.store.endRebuild();

  REQUIRE(tree != nullptr);

  std::vector<Size> rectSizes{};
  std::function<void(SceneNode const&)> walk = [&](SceneNode const& node) {
    if (auto const* rect = dynamic_cast<RectSceneNode const*>(&node)) {
      rectSizes.push_back(rect->size);
    }
    for (std::unique_ptr<SceneNode> const& child : node.children()) {
      walk(*child);
    }
  };
  walk(*tree);

  auto hasRectSize = [&](float width, float height) {
    return std::any_of(rectSizes.begin(), rectSizes.end(), [&](Size const& size) {
      return size.width == doctest::Approx(width) && size.height == doctest::Approx(height);
    });
  };

  CHECK(hasRectSize(320.f, 180.f));
  CHECK(hasRectSize(320.f, 220.f));
  CHECK(hasRectSize(320.f, 120.f));
}

TEST_CASE("SceneBuilder: modifier chrome repaints when child bounds change across reuse") {
  VariableTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints narrowConstraints{};
  narrowConstraints.maxWidth = 120.f;
  narrowConstraints.maxHeight = 80.f;

  LayoutConstraints wideConstraints{};
  wideConstraints.maxWidth = 300.f;
  wideConstraints.maxHeight = 80.f;

  auto makeLabel = [](std::string text) -> Element {
    return Text{
        .text = std::move(text),
        .horizontalAlignment = HorizontalAlignment::Center,
        .verticalAlignment = VerticalAlignment::Center,
    }
        .padding(6.f, 10.f, 6.f, 10.f)
        .fill(FillStyle::solid(Colors::black));
  };

  std::unique_ptr<SceneNode> tree = builder.build(makeLabel("Short"), NodeId{1ull}, narrowConstraints);
  REQUIRE(tree != nullptr);

  NullRenderer firstRenderer{};
  render(*tree, firstRenderer);
  REQUIRE(firstRenderer.rects.size() == 1);
  float const firstWidth = firstRenderer.rects.front().width;

  tree = builder.build(makeLabel("A much longer label"), NodeId{1ull}, wideConstraints, std::move(tree));
  REQUIRE(tree != nullptr);

  NullRenderer secondRenderer{};
  render(*tree, secondRenderer);
  REQUIRE(secondRenderer.rects.size() == 1);
  CHECK(secondRenderer.rects.front().width > firstWidth);
}

TEST_CASE("SceneBuilder: geometry index records assigned frames by keyed path") {
  NullTextSystem textSystem{};
  SceneGeometryIndex geometry{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &geometry};
  LayoutConstraints constraints{};
  constraints.maxWidth = 120.f;
  constraints.maxHeight = 120.f;

  Element root = VStack{
      .spacing = 8.f,
      .children = {
          keyedRect("a", 20.f, 10.f),
          keyedRect("b", 30.f, 15.f),
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(root, NodeId{1ull}, constraints);
  REQUIRE(tree);

  std::optional<Rect> aRect = geometry.rectForKey(ComponentKey{LocalId::fromString("a")});
  std::optional<Rect> bRect = geometry.rectForKey(ComponentKey{LocalId::fromString("b")});
  REQUIRE(aRect.has_value());
  REQUIRE(bRect.has_value());
  CHECK(aRect->x == doctest::Approx(0.f));
  CHECK(aRect->y == doctest::Approx(43.5f));
  CHECK(aRect->width == doctest::Approx(20.f));
  CHECK(aRect->height == doctest::Approx(10.f));
  CHECK(bRect->x == doctest::Approx(0.f));
  CHECK(bRect->y == doctest::Approx(61.5f));
  CHECK(bRect->width == doctest::Approx(30.f));
  CHECK(bRect->height == doctest::Approx(15.f));
}

TEST_CASE("SceneBuilder: clean composite subtree is skipped and geometry is retained across parent rebuilds") {
  CountingTextSystem textSystem{};
  SceneGeometryIndex geometry{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &geometry};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 120.f;

  Signal<int> tickSignal{0};
  State<int> tick{&tickSignal};
  int parentCalls = 0;
  int childCalls = 0;

  auto makeRoot = [&]() -> Element {
    return Element{RetainedParent{
        .tick = tick,
        .parentCalls = &parentCalls,
        .childCalls = &childCalls,
    }};
  };

  scope.store.beginRebuild(true);
  std::unique_ptr<SceneNode> tree = builder.build(makeRoot(), NodeId{1ull}, constraints);
  scope.store.endRebuild();

  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);
  SceneNode* retainedChild = tree->children()[1].get();
  REQUIRE(dynamic_cast<TextSceneNode*>(retainedChild) != nullptr);
  int const initialChildCalls = childCalls;
  int const initialParentCalls = parentCalls;
  int const initialLayoutCount = textSystem.layoutCount;
  REQUIRE(initialChildCalls > 0);
  REQUIRE(initialParentCalls > 0);
  REQUIRE(initialLayoutCount > 0);

  std::optional<Rect> beforeRect = geometry.forKey(ComponentKey{LocalId::fromString("child")});
  REQUIRE(beforeRect.has_value());

  tick = 12;

  scope.store.beginRebuild(false);
  tree = builder.build(makeRoot(), NodeId{1ull}, constraints, std::move(tree));
  scope.store.endRebuild();

  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);
  CHECK(tree->children()[1].get() == retainedChild);
  CHECK(parentCalls > initialParentCalls);
  CHECK(childCalls == initialChildCalls);
  CHECK(textSystem.layoutCount == initialLayoutCount);

  std::optional<Rect> afterRect = geometry.forKey(ComponentKey{LocalId::fromString("child")});
  REQUIRE(afterRect.has_value());
  CHECK(afterRect->x == doctest::Approx(beforeRect->x));
  CHECK(afterRect->y > beforeRect->y);
}

TEST_CASE("SceneBuilder: centered text keeps its assigned box for boxed layout") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.minWidth = 320.f;
  constraints.minHeight = 320.f;
  constraints.maxWidth = 320.f;
  constraints.maxHeight = 320.f;

  Element text = Text{
      .text = "Hello, World!",
      .horizontalAlignment = HorizontalAlignment::Center,
      .verticalAlignment = VerticalAlignment::Center,
  };

  std::unique_ptr<SceneNode> tree = builder.build(text, NodeId{1ull}, constraints);
  auto* textNode = dynamic_cast<TextSceneNode*>(tree.get());
  REQUIRE(textNode != nullptr);
  CHECK(textNode->allocation.width == doctest::Approx(320.f));
  CHECK(textNode->allocation.height == doctest::Approx(320.f));

  NullRenderer renderer{};
  render(*tree, renderer);
  CHECK(renderer.textCount == 1);
}

TEST_CASE("SceneBuilder: padded text uses the assigned slot's inner content box for layout") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 80.f;

  Element text = Text{
      .text = "Hello, World!",
      .horizontalAlignment = HorizontalAlignment::Center,
      .verticalAlignment = VerticalAlignment::Center,
  }
                     .padding(8.f, 12.f, 8.f, 12.f)
                     .fill(FillStyle::solid(Colors::black));

  std::unique_ptr<SceneNode> tree = builder.build(text, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);

  auto* wrapper = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(wrapper != nullptr);
  REQUIRE(wrapper->children().size() == 1);

  SceneNode* layoutWrapper = wrapper->children()[0].get();
  REQUIRE(layoutWrapper != nullptr);
  REQUIRE(layoutWrapper->children().size() == 1);

  auto* textNode = dynamic_cast<TextSceneNode*>(layoutWrapper->children()[0].get());
  REQUIRE(textNode != nullptr);
  CHECK(wrapper->bounds.width == doctest::Approx(200.f));
  CHECK(wrapper->bounds.height == doctest::Approx(80.f));
  CHECK(textNode->allocation.width == doctest::Approx(176.f));
  CHECK(textNode->allocation.height == doctest::Approx(64.f));
}

TEST_CASE("SceneBuilder: badge background rectangle keeps its assigned ZStack slot") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 48.2f;
  constraints.maxHeight = 39.f;

  Element badge = ZStack{
      .horizontalAlignment = Alignment::Center,
      .verticalAlignment = Alignment::Center,
      .children = children(
          Rectangle{}.fill(FillStyle::solid(Colors::black)).cornerRadius(CornerRadius{10.f}),
          Text{
              .text = "Saved",
              .horizontalAlignment = HorizontalAlignment::Center,
              .verticalAlignment = VerticalAlignment::Center,
          }
              .padding(6.f, 10.f, 6.f, 10.f)),
  };

  std::unique_ptr<SceneNode> tree = builder.build(badge, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);

  auto findRect = [&](this auto const& self, SceneNode* node) -> RectSceneNode* {
    if (!node) {
      return nullptr;
    }
    if (auto* rect = dynamic_cast<RectSceneNode*>(node)) {
      return rect;
    }
    for (std::unique_ptr<SceneNode> const& child : node->children()) {
      if (RectSceneNode* rect = self(child.get())) {
        return rect;
      }
    }
    return nullptr;
  };

  auto* rectNode = findRect(tree.get());
  REQUIRE(rectNode != nullptr);
  CHECK(rectNode->size.width == doctest::Approx(48.2f));
  CHECK(rectNode->size.height == doctest::Approx(39.f));
}

TEST_CASE("SceneBuilder: scale around center keeps the parent-assigned slot") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 414.f;
  constraints.maxHeight = 49.f;

  Element scaled = ScaleAroundCenter{
      .scale = 1.f,
      .child = Text{
          .text = "Create Invoice",
          .horizontalAlignment = HorizontalAlignment::Center,
          .verticalAlignment = VerticalAlignment::Center,
      }
                   .fill(FillStyle::solid(Colors::black))
                   .padding(16.f),
  };

  std::unique_ptr<SceneNode> tree = builder.build(scaled, NodeId{1ull}, constraints);
  auto* transformNode = dynamic_cast<CustomTransformSceneNode*>(tree.get());
  REQUIRE(transformNode != nullptr);
  CHECK(transformNode->bounds.width == doctest::Approx(414.f));
  CHECK(transformNode->bounds.height == doctest::Approx(49.f));
  REQUIRE(transformNode->children().size() == 1);

  auto* wrapper = dynamic_cast<ModifierSceneNode*>(transformNode->children()[0].get());
  REQUIRE(wrapper != nullptr);
  CHECK(wrapper->children()[0]->bounds.width == doctest::Approx(414.f));
  CHECK(wrapper->children()[0]->bounds.height == doctest::Approx(49.f));
}

TEST_CASE("SceneBuilder: stretched flex HStack leaves adopt their assigned slot size") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 100.f;
  constraints.maxHeight = 50.f;

  Element row = HStack{
      .alignment = Alignment::Stretch,
      .children = {
          Element{Rectangle{}}.size(20.f, 10.f).flex(1.f),
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(row, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  auto findRect = [&](this auto const& self, SceneNode* node) -> RectSceneNode* {
    if (!node) {
      return nullptr;
    }
    if (auto* rect = dynamic_cast<RectSceneNode*>(node)) {
      return rect;
    }
    for (std::unique_ptr<SceneNode> const& child : node->children()) {
      if (RectSceneNode* rect = self(child.get())) {
        return rect;
      }
    }
    return nullptr;
  };
  auto* rectNode = findRect(tree.get());
  REQUIRE(rectNode != nullptr);
  CHECK(rectNode->size.width == doctest::Approx(100.f));
  CHECK(rectNode->size.height == doctest::Approx(50.f));
}

TEST_CASE("SceneBuilder: constrained stacks report their assigned slot instead of growing with overflowing content") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 198.f;
  constraints.maxHeight = 180.f;

  Element column = VStack{
      .alignment = Alignment::Center,
      .children = {
          HStack{
              .spacing = 0.f,
              .children = {
                  Text{.text = "Leading"},
                  Element{Spacer{}},
                  Text{.text = "Spacer pushes this trailing label"},
              },
          },
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(column, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  CHECK(tree->bounds.width == doctest::Approx(198.f));
  CHECK(tree->bounds.height == doctest::Approx(180.f));
  REQUIRE(tree->children().size() == 1);
  CHECK(tree->children()[0]->position.x == doctest::Approx(0.f));
  CHECK(tree->children()[0]->bounds.width == doctest::Approx(198.f));
}

TEST_CASE("SceneBuilder: non-stretch HStack children keep their intrinsic height slot") {
  VariableTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 640.f;
  constraints.maxHeight = 320.f;

  Element row = HStack{
      .spacing = 16.f,
      .alignment = Alignment::Start,
      .children = children(
          Element{VStack{
              .spacing = 12.f,
              .alignment = Alignment::Start,
              .children = {
                  Text{.text = "Section title"},
                  Text{.text = "Section body copy that should stay top aligned."},
              },
          }}
              .flex(1.f, 1.f, 0.f),
          Element{Rectangle{}}.size(24.f, 24.f)),
  };

  std::unique_ptr<SceneNode> tree = builder.build(row, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);

  SceneNode const& card = *tree->children()[0];
  REQUIRE(card.children().size() == 2);
  CHECK(card.bounds.height < 200.f);
  CHECK(card.children()[0]->position.y == doctest::Approx(0.f));
  CHECK(card.children()[1]->position.y > card.children()[0]->position.y);
}

TEST_CASE("SceneBuilder: centered ZStack overlay is not centered twice") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 220.f;
  constraints.maxHeight = 180.f;

  Element overlay = ZStack{
      .horizontalAlignment = Alignment::Center,
      .verticalAlignment = Alignment::Center,
      .children = {
          Element{Rectangle{}}.size(0.f, 180.f),
          Element{Rectangle{}}.size(220.f, 104.f),
          Element{VStack{
              .spacing = 4.f,
              .alignment = Alignment::Center,
              .children = {
                  Text{.text = "Overlay content", .horizontalAlignment = HorizontalAlignment::Center},
                  Text{.text = "Centered inside a shared layer", .horizontalAlignment = HorizontalAlignment::Center},
              },
          }},
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(overlay, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 3);
  SceneNode* overlayColumn = tree->children()[2].get();
  CHECK(overlayColumn->position.x == doctest::Approx(0.f));
  CHECK(overlayColumn->position.y == doctest::Approx(0.f));
  CHECK(overlayColumn->bounds.width == doctest::Approx(220.f));
  CHECK(overlayColumn->bounds.height == doctest::Approx(180.f));
}

TEST_CASE("SceneBuilder: centered VStack overflow is allowed to go negative") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 120.f;
  constraints.maxHeight = 80.f;

  Element column = VStack{
      .alignment = Alignment::Center,
      .children = {
          Element{Rectangle{}}.size(160.f, 24.f),
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(column, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 1);
  CHECK(tree->bounds.width == doctest::Approx(120.f));
  CHECK(tree->children()[0]->position.x == doctest::Approx(-20.f));
}

TEST_CASE("SceneBuilder: HStack flexed explicit leaves stay collapsed when resized narrower") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  auto makeRow = []() -> Element {
    return HStack{
        .spacing = 12.f,
        .alignment = Alignment::Stretch,
        .children = children(
            Element{Rectangle{}}.size(56.f, 54.f).flex(2.f, 1.f, 0.f),
            Element{Rectangle{}}.size(56.f, 76.f),
            Element{Rectangle{}}.size(56.f, 40.f).flex(1.f, 1.f, 0.f),
            Element{Rectangle{}}.size(56.f, 54.f)),
    };
  };

  auto findLeafRects = [](SceneNode const& root) {
    std::vector<RectSceneNode const*> rects{};
    std::function<void(SceneNode const&)> walk = [&](SceneNode const& node) {
      if (auto const* rect = dynamic_cast<RectSceneNode const*>(&node)) {
        rects.push_back(rect);
      }
      for (std::unique_ptr<SceneNode> const& child : node.children()) {
        walk(*child);
      }
    };
    walk(root);
    return rects;
  };

  LayoutConstraints wider{};
  wider.maxWidth = 151.f;
  wider.maxHeight = 76.f;

  LayoutConstraints narrower{};
  narrower.maxWidth = 145.f;
  narrower.maxHeight = 76.f;

  std::unique_ptr<SceneNode> tree = builder.build(makeRow(), NodeId{1ull}, wider);
  REQUIRE(tree != nullptr);
  std::vector<RectSceneNode const*> rects = findLeafRects(*tree);
  REQUIRE(rects.size() == 4);
  CHECK(rects[0]->size.width == doctest::Approx(1.5f));
  CHECK(rects[1]->size.width == doctest::Approx(56.f));
  CHECK(rects[2]->size.width == doctest::Approx(1.5f));
  CHECK(rects[3]->size.width == doctest::Approx(56.f));

  tree = builder.build(makeRow(), NodeId{1ull}, narrower, std::move(tree));
  REQUIRE(tree != nullptr);
  rects = findLeafRects(*tree);
  REQUIRE(rects.size() == 4);
  CHECK(rects[0]->size.width == doctest::Approx(0.f));
  CHECK(rects[1]->size.width == doctest::Approx(56.f));
  CHECK(rects[2]->size.width == doctest::Approx(0.f));
  CHECK(rects[3]->size.width == doctest::Approx(56.f));
}

TEST_CASE("SceneBuilder: layout demo cards shrink on narrow rebuild instead of retaining wide widths") {
  VariableTextSystem textSystem{};
  SceneGeometryIndex geometry{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &geometry};

  LayoutConstraints wide{};
  wide.maxWidth = 960.f;
  wide.maxHeight = 920.f;

  LayoutConstraints narrow{};
  narrow.maxWidth = 294.f;
  narrow.maxHeight = 921.f;

  std::unique_ptr<SceneNode> tree = builder.build(demoLayoutRoot(), NodeId{1ull}, wide);
  REQUIRE(tree != nullptr);
  tree = builder.build(demoLayoutRoot(), NodeId{1ull}, narrow, std::move(tree));
  REQUIRE(tree != nullptr);

  std::optional<Rect> rootContent = geometry.rectForKey(ComponentKey{LocalId::fromIndex(0)});
  std::optional<Rect> vstackCard = geometry.rectForKey(ComponentKey{LocalId::fromIndex(0), LocalId::fromIndex(2)});
  std::optional<Rect> vstackCardContent =
      geometry.rectForKey(ComponentKey{LocalId::fromIndex(0), LocalId::fromIndex(2), LocalId::fromIndex(2)});
  std::optional<Rect> vstackNestedRow = geometry.rectForKey(
      ComponentKey{LocalId::fromIndex(0), LocalId::fromIndex(2), LocalId::fromIndex(2), LocalId::fromIndex(3)});
  std::optional<Rect> hstackNestedRow = geometry.rectForKey(
      ComponentKey{LocalId::fromIndex(0), LocalId::fromIndex(3), LocalId::fromIndex(2), LocalId::fromIndex(1)});

  REQUIRE(rootContent.has_value());
  REQUIRE(vstackCard.has_value());
  REQUIRE(vstackCardContent.has_value());
  REQUIRE(vstackNestedRow.has_value());
  REQUIRE(hstackNestedRow.has_value());

  CHECK(rootContent->width == doctest::Approx(294.f));
  CHECK(vstackCard->width == doctest::Approx(254.f));
  CHECK(vstackCardContent->width == doctest::Approx(222.f));
  CHECK(vstackNestedRow->width == doctest::Approx(198.f));
  CHECK(hstackNestedRow->width == doctest::Approx(198.f));

  REQUIRE(tree->children().size() == 1);
  SceneNode const* scrollViewport = tree->children()[0].get();
  REQUIRE(scrollViewport->children().size() >= 1);
  SceneNode const* scrollContent = scrollViewport->children()[0].get();
  CHECK(scrollContent->bounds.x >= doctest::Approx(0.f));
  CHECK(scrollContent->bounds.width <= doctest::Approx(294.f));
}

TEST_CASE("SceneBuilder: layout demo overlay text column stays centered after narrow rebuild") {
  VariableTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints wide{};
  wide.maxWidth = 960.f;
  wide.maxHeight = 920.f;

  LayoutConstraints narrow{};
  narrow.maxWidth = 294.f;
  narrow.maxHeight = 921.f;

  std::unique_ptr<SceneNode> tree = builder.build(demoZStackCard(), NodeId{1ull}, wide);
  REQUIRE(tree != nullptr);
  tree = builder.build(demoZStackCard(), NodeId{1ull}, narrow, std::move(tree));
  REQUIRE(tree != nullptr);

  std::function<SceneNode const*(SceneNode const*)> findOverlayStack = [&](SceneNode const* node) -> SceneNode const* {
    if (!node) {
      return nullptr;
    }
    if (node->kind() == SceneNodeKind::Group && node->children().size() == 3 &&
        node->children()[0]->kind() == SceneNodeKind::Rect &&
        node->children()[1]->kind() == SceneNodeKind::Rect) {
      return node;
    }
    for (std::unique_ptr<SceneNode> const& child : node->children()) {
      if (SceneNode const* match = findOverlayStack(child.get())) {
        return match;
      }
    }
    return nullptr;
  };

  SceneNode const* overlayStack = findOverlayStack(tree.get());
  REQUIRE(overlayStack != nullptr);
  SceneNode const* overlayColumn = overlayStack->children()[2].get();
  CHECK(overlayColumn->position.x == doctest::Approx(0.f));
  CHECK(overlayColumn->position.y == doctest::Approx(0.f));
  CHECK(overlayColumn->bounds.width == doctest::Approx(238.f));
  CHECK(overlayColumn->bounds.height == doctest::Approx(180.f));
}

TEST_CASE("SceneGeometryIndex: committed queries use current frames and previous-frame fallback") {
  SceneGeometryIndex geometry{};
  ComponentKey const parentKey{LocalId::fromString("parent")};
  ComponentKey const childKey{LocalId::fromString("parent"), LocalId::fromString("child")};
  ComponentKey const grandchildKey{
      LocalId::fromString("parent"),
      LocalId::fromString("child"),
      LocalId::fromString("grandchild"),
  };
  ComponentKey const removedKey{LocalId::fromString("removed")};

  geometry.beginBuild();
  geometry.record(parentKey, Rect{1.f, 2.f, 30.f, 40.f});
  geometry.record(childKey, Rect{3.f, 4.f, 10.f, 12.f});
  geometry.record(removedKey, Rect{7.f, 8.f, 9.f, 10.f});
  geometry.finishBuild();

  {
    StoreScope scope{};
    scope.store.pushComponent(parentKey, std::type_index(typeid(HelloRoot)));
    std::optional<Rect> currentRect = geometry.forCurrentComponent(scope.store);
    REQUIRE(currentRect.has_value());
    CHECK(currentRect->x == doctest::Approx(1.f));
    CHECK(currentRect->y == doctest::Approx(2.f));
    scope.store.popComponent();
  }

  std::optional<Rect> initialPrefixRect = geometry.forLeafKeyPrefix(grandchildKey);
  REQUIRE(initialPrefixRect.has_value());
  CHECK(initialPrefixRect->x == doctest::Approx(3.f));
  CHECK(initialPrefixRect->y == doctest::Approx(4.f));

  geometry.beginBuild();
  geometry.record(parentKey, Rect{11.f, 12.f, 30.f, 40.f});
  geometry.finishBuild();

  std::optional<Rect> currentRect = geometry.forKey(parentKey);
  REQUIRE(currentRect.has_value());
  CHECK(currentRect->x == doctest::Approx(11.f));
  CHECK(currentRect->y == doctest::Approx(12.f));

  std::optional<Rect> removedRect = geometry.forKey(removedKey);
  REQUIRE(removedRect.has_value());
  CHECK(removedRect->x == doctest::Approx(7.f));
  CHECK(removedRect->y == doctest::Approx(8.f));

  std::optional<Rect> prefixRect = geometry.forTapAnchor(grandchildKey);
  REQUIRE(prefixRect.has_value());
  CHECK(prefixRect->x == doctest::Approx(3.f));
  CHECK(prefixRect->y == doctest::Approx(4.f));
}

TEST_CASE("SceneBuilder: rectangle retains modifier paint on the primitive node") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 120.f;
  constraints.maxHeight = 120.f;

  Element rect = Element{Rectangle{}}
                     .size(20.f, 10.f)
                     .fill(FillStyle::solid(Color::hex(0x3366cc)));

  std::unique_ptr<SceneNode> tree = builder.build(rect, NodeId{1ull}, constraints);
  auto* rectNode = dynamic_cast<RectSceneNode*>(tree.get());
  REQUIRE(rectNode != nullptr);
  CHECK_FALSE(rectNode->fill.isNone());

  NullRenderer renderer{};
  render(*tree, renderer);
  CHECK(renderer.rectCount == 1);
}

TEST_CASE("SceneBuilder: PopoverCalloutShape builds retained chrome and content nodes") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 200.f;

  Element popover = PopoverCalloutShape{
      .placement = PopoverPlacement::Below,
      .arrow = true,
      .padding = 12.f,
      .cornerRadius = CornerRadius{10.f},
      .backgroundColor = Color::hex(0xFFFFFF),
      .borderColor = Color::hex(0xE0E0E6),
      .borderWidth = 1.5f,
      .content = Element{Rectangle{}}.size(40.f, 20.f),
  };

  std::unique_ptr<SceneNode> tree = builder.build(popover, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);

  auto* chrome = dynamic_cast<PathSceneNode*>(tree->children()[0].get());
  auto* content = dynamic_cast<RectSceneNode*>(tree->children()[1].get());
  REQUIRE(chrome != nullptr);
  REQUIRE(content != nullptr);
  CHECK_FALSE(chrome->fill.isNone());
  CHECK(chrome->stroke.width == doctest::Approx(1.5f));
  CHECK(content->position.x == doctest::Approx(12.f));
  CHECK(content->position.y == doctest::Approx(PopoverCalloutShape::kArrowH + 12.f));
  CHECK(tree->bounds.height == doctest::Approx(20.f + 24.f + PopoverCalloutShape::kArrowH));

  NullRenderer renderer{};
  render(*tree, renderer);
  CHECK(renderer.pathCount == 1);
}

TEST_CASE("SceneBuilder: composite root exposes a retained scene body with the runtime root key") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};

  flux::TypedRootHolder<HelloRoot> holder{std::in_place};

  flux::LayoutConstraints constraints{};
  constraints.minWidth = 320.f;
  constraints.minHeight = 320.f;
  constraints.maxWidth = 320.f;
  constraints.maxHeight = 320.f;

  scope.store.beginRebuild(true);
  holder.prepareSceneElement(constraints);

  Element const* sceneRoot = holder.sceneElementForCurrentBuild();
  REQUIRE(sceneRoot != nullptr);
  CHECK(sceneRoot->typeTag() == ElementType::Text);
  CHECK(holder.sceneRootKey() == ComponentKey{LocalId::fromIndex(0), LocalId::fromIndex(0)});

  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  std::unique_ptr<SceneNode> tree =
      builder.build(*sceneRoot, NodeId{1ull}, constraints, nullptr, holder.sceneRootKey());
  REQUIRE(tree != nullptr);

  NullRenderer renderer{};
  render(*tree, renderer);
  CHECK(renderer.textCount == 1);

  scope.store.endRebuild();
}

TEST_CASE("SceneTree interaction lookup preserves focus order and keyed handler lookup") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 120.f;

  bool keyDownA = false;
  bool keyDownB = false;
  Element root = VStack{
      .spacing = 4.f,
      .children = {
          keyedRect("a", 40.f, 20.f)
              .focusable(true)
              .onKeyDown([&](KeyCode, Modifiers) { keyDownA = true; }),
          keyedRect("b", 40.f, 20.f)
              .focusable(true)
              .onKeyDown([&](KeyCode, Modifiers) { keyDownB = true; }),
      },
  };

  SceneTree tree{builder.build(root, NodeId{1ull}, constraints)};

  std::vector<ComponentKey> const order = collectFocusableKeys(tree);
  REQUIRE(order.size() == 2);
  CHECK(order[0] == ComponentKey{LocalId::fromString("a")});
  CHECK(order[1] == ComponentKey{LocalId::fromString("b")});

  auto const [idA, interactionA] = findInteractionByKey(tree, ComponentKey{LocalId::fromString("a")});
  REQUIRE(idA.isValid());
  REQUIRE(interactionA != nullptr);
  REQUIRE(interactionA->focusable);
  REQUIRE(static_cast<bool>(interactionA->onKeyDown));
  interactionA->onKeyDown(KeyCode{}, Modifiers{});
  CHECK(keyDownA);

  auto const [closestId, closestInteraction] = findClosestInteractionByKey(
      tree, ComponentKey{LocalId::fromString("b"), LocalId::fromString("child")});
  REQUIRE(closestId.isValid());
  REQUIRE(closestInteraction != nullptr);
  REQUIRE(static_cast<bool>(closestInteraction->onKeyDown));
  closestInteraction->onKeyDown(KeyCode{}, Modifiers{});
  CHECK(keyDownB);
}

TEST_CASE("SceneTree interaction hit testing respects custom transform local coordinates") {
  auto root = std::make_unique<SceneNode>(NodeId{1ull});
  auto transform = std::make_unique<CustomTransformSceneNode>(NodeId{2ull});
  transform->transform = Mat3::scale(2.f);

  auto rect = std::make_unique<RectSceneNode>(NodeId{3ull});
  rect->position = Point{4.f, 3.f};
  rect->size = Size{20.f, 10.f};
  auto interaction = std::make_unique<InteractionData>();
  interaction->stableTargetKey = ComponentKey{LocalId::fromString("scaled")};
  rect->setInteraction(std::move(interaction));
  rect->recomputeBounds();

  RectSceneNode* rectPtr = rect.get();
  transform->appendChild(std::move(rect));
  transform->recomputeBounds();
  root->appendChild(std::move(transform));
  root->recomputeBounds();

  SceneTree tree{std::move(root)};

  auto const hit = hitTestInteraction(tree, Point{18.f, 16.f});
  REQUIRE(hit.has_value());
  CHECK(hit->nodeId == rectPtr->id());
  CHECK(hit->localPoint.x == doctest::Approx(5.f));
  CHECK(hit->localPoint.y == doctest::Approx(5.f));
  REQUIRE(hit->interaction != nullptr);
  CHECK(hit->interaction->stableTargetKey == ComponentKey{LocalId::fromString("scaled")});

  auto const local = HitTester{}.localPointForNode(tree, Point{18.f, 16.f}, rectPtr->id());
  REQUIRE(local.has_value());
  CHECK(local->x == doctest::Approx(5.f));
  CHECK(local->y == doctest::Approx(5.f));
}

TEST_CASE("GestureTracker: overlay-scoped taps resolve through the overlay scene tree") {
  GestureTracker tracker{};

  bool mainTapped = false;
  InteractiveRectTree main = makeInteractiveRectTree("shared", false, [&] { mainTapped = true; });

  bool overlayTapped = false;
  ComponentKey observedTapKey{};
  std::optional<OverlayId> observedOverlayScope{};
  InteractiveRectTree overlayTree = makeInteractiveRectTree("shared", false, [&] {
    overlayTapped = true;
    observedTapKey = tracker.pendingTapLeafKey();
    observedOverlayScope = tracker.pendingTapOverlayScope();
  });

  OverlayEntry overlay{};
  overlay.id = OverlayId{42ull};
  overlay.sceneTree = std::move(overlayTree.tree);
  std::vector<OverlayEntry const*> overlays{&overlay};

  GestureTracker::PressState released{};
  released.stableTargetKey = ComponentKey{LocalId::fromString("shared")};
  released.hadOnTapOnDown = true;
  released.overlayScope = overlay.id;

  CHECK(tracker.dispatchTap(released, overlays, main.tree));
  CHECK(overlayTapped);
  CHECK_FALSE(mainTapped);
  CHECK(observedTapKey == released.stableTargetKey);
  REQUIRE(observedOverlayScope.has_value());
  CHECK(observedOverlayScope->value == overlay.id.value);
  CHECK(tracker.pendingTapLeafKey().empty());
  CHECK_FALSE(tracker.pendingTapOverlayScope().has_value());
}

TEST_CASE("GestureTracker: overlay press lookup falls back by stable key inside the overlay tree") {
  GestureTracker tracker{};
  InteractiveRectTree main = makeInteractiveRectTree("shared");
  InteractiveRectTree overlayTree = makeInteractiveRectTree("shared");

  OverlayEntry overlay{};
  overlay.id = OverlayId{7ull};
  overlay.sceneTree = std::move(overlayTree.tree);
  std::vector<OverlayEntry const*> overlays{&overlay};

  GestureTracker::PressState press{};
  press.nodeId = NodeId{999ull};
  press.stableTargetKey = ComponentKey{LocalId::fromString("shared")};
  press.overlayScope = overlay.id;

  auto const [resolvedId, interaction] = tracker.findPressInteraction(press, overlays, main.tree);
  REQUIRE(interaction != nullptr);
  CHECK(resolvedId == overlayTree.leafId);
  CHECK(interaction->stableTargetKey == press.stableTargetKey);
  CHECK(tracker.sceneTreeForPress(press, overlays, main.tree) == &overlay.sceneTree);
}

TEST_CASE("FocusController: modal overlay rebuild syncs focus from the retained overlay tree") {
  FocusController focus{};
  InteractiveRectTree overlayTree = makeInteractiveRectTree("dialog-primary", true);

  OverlayEntry overlay{};
  overlay.id = OverlayId{9ull};
  overlay.config.modal = true;
  overlay.sceneTree = std::move(overlayTree.tree);

  focus.onOverlayPushed(overlay);
  focus.syncAfterOverlayRebuild(overlay);

  REQUIRE(focus.focusInOverlay().has_value());
  CHECK(focus.focusInOverlay()->value == overlay.id.value);
  CHECK(focus.focusedKey() == ComponentKey{LocalId::fromString("dialog-primary")});
}
