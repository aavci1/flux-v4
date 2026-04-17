#include <doctest/doctest.h>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/Renderer.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/SceneGeometryIndex.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <memory>
#include <string_view>

namespace {

using namespace flux;

class NullTextSystem final : public TextSystem {
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

class NullRenderer final : public Renderer {
public:
  void save() override {}
  void restore() override {}
  void translate(Point) override {}
  void transform(Mat3 const&) override {}
  void clipRect(Rect, bool = false) override {}
  bool quickReject(Rect) const override { return false; }
  void setOpacity(float) override {}
  void setBlendMode(BlendMode) override {}
  void drawRect(Rect const&, CornerRadius const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override {}
  void drawLine(Point, Point, StrokeStyle const&) override {}
  void drawPath(Path const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override {}
  void drawTextLayout(TextLayout const&, Point) override {}
  void drawImage(Image const&, Rect const&, ImageFillMode, CornerRadius const&, float) override {}
};

struct EnvironmentScope {
  explicit EnvironmentScope(EnvironmentLayer layer) {
    EnvironmentStack::current().push(std::move(layer));
  }
  ~EnvironmentScope() { EnvironmentStack::current().pop(); }
};

Element keyedRect(std::string key, float width, float height) {
  return Element{Rectangle{}}.key(std::move(key)).size(width, height);
}

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
  SceneNode* scrollCore = tree.get();
  REQUIRE(scrollCore->children().size() == 1);
  SceneNode* contentGroup = scrollCore->children()[0].get();
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
  scrollCore = tree.get();
  REQUIRE(scrollCore->children().size() == 1);
  contentGroup = scrollCore->children()[0].get();
  REQUIRE(contentGroup->children().size() == 2);

  CHECK(contentGroup->children()[0].get() == firstA);
  CHECK(contentGroup->children()[1].get() == firstB);
  CHECK(contentGroup->children()[0]->position.y == doctest::Approx(-12.f));
  CHECK(contentGroup->children()[1]->position.y == doctest::Approx(18.f));
  CHECK_FALSE(firstA->paintDirty());
  CHECK_FALSE(firstB->paintDirty());
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
