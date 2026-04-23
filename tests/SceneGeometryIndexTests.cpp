#include "SceneBuilderTestSupport.hpp"

namespace {

struct DecoratedBodyComponent {
  Element body() const {
    return Text{
               .text = "decorated",
               .horizontalAlignment = HorizontalAlignment::Leading,
           }
        .padding(5.f, 10.f, 5.f, 10.f)
        .stroke(StrokeStyle::solid(Color::primary(), 1.f))
        .cornerRadius(CornerRadius{4.f});
  }
};

struct InnerBodyField {
  Element body() const {
    return HStack{
        .spacing = 8.f,
        .alignment = Alignment::Center,
        .children = children(demoColorBlock(20.f, 10.f), demoColorBlock(12.f, 12.f)),
    }
        .padding(10.f, 14.f, 10.f, 14.f)
        .stroke(StrokeStyle::solid(Color::primary(), 1.f))
        .cornerRadius(CornerRadius{4.f});
  }
};

struct OuterBodyField {
  Element body() const { return Element{InnerBodyField{}}; }
};

} // namespace

TEST_CASE("SceneBuilder records assigned frames in the scenegraph by keyed path") {
  NullTextSystem textSystem{};
  scenegraph::SceneGraph graph{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &graph};
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

  graph.setRoot(builder.build(root, constraints));

  std::optional<Rect> aRect = graph.rectForKey(ComponentKey{LocalId::fromString("a")});
  std::optional<Rect> bRect = graph.rectForKey(ComponentKey{LocalId::fromString("b")});
  REQUIRE(aRect.has_value());
  REQUIRE(bRect.has_value());
  CHECK(aRect->x == doctest::Approx(50.f));
  CHECK(aRect->y == doctest::Approx(0.f));
  CHECK(aRect->width == doctest::Approx(20.f));
  CHECK(aRect->height == doctest::Approx(10.f));
  CHECK(bRect->x == doctest::Approx(45.f));
  CHECK(bRect->y == doctest::Approx(18.f));
  CHECK(bRect->width == doctest::Approx(30.f));
  CHECK(bRect->height == doctest::Approx(15.f));
}

TEST_CASE("SceneBuilder geometry tracks centered VStack child cross-axis offsets") {
  NullTextSystem textSystem{};
  scenegraph::SceneGraph graph{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &graph};

  LayoutConstraints constraints{};
  constraints.maxWidth = 100.f;
  constraints.maxHeight = 60.f;

  Element centeredColumn = VStack{
      .alignment = Alignment::Center,
      .children = {
          keyedRect("column-child", 20.f, 10.f),
      },
  };

  graph.setRoot(builder.build(centeredColumn, constraints));

  std::optional<Rect> columnChildRect =
      graph.rectForKey(ComponentKey{LocalId::fromString("column-child")});
  REQUIRE(columnChildRect.has_value());
  CHECK(columnChildRect->x == doctest::Approx(40.f));
  CHECK(columnChildRect->y == doctest::Approx(0.f));
  CHECK(columnChildRect->width == doctest::Approx(20.f));
  CHECK(columnChildRect->height == doctest::Approx(10.f));
}

TEST_CASE("SceneBuilder geometry records outer layout rect for body-expanded components") {
  NullTextSystem textSystem{};
  scenegraph::SceneGraph graph{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &graph};

  LayoutConstraints constraints{};
  constraints.maxWidth = 120.f;
  constraints.maxHeight = 120.f;

  Element root = VStack{
      .spacing = 8.f,
      .children = {
          demoColorBlock(120.f, 20.f),
          Element{DecoratedBodyComponent{}}.key("field"),
      },
  };

  graph.setRoot(builder.build(root, constraints));

  std::optional<Rect> fieldRect = graph.rectForKey(ComponentKey{LocalId::fromString("field")});
  REQUIRE(fieldRect.has_value());
  CHECK(fieldRect->x == doctest::Approx(14.f));
  CHECK(fieldRect->y == doctest::Approx(28.f));
  CHECK(fieldRect->width == doctest::Approx(92.f));
  CHECK(fieldRect->height == doctest::Approx(24.f));
}

TEST_CASE("SceneBuilder body wrapper geometry does not collide with first content child") {
  NullTextSystem textSystem{};
  scenegraph::SceneGraph graph{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &graph};

  LayoutConstraints constraints{};
  constraints.maxWidth = 120.f;
  constraints.maxHeight = 120.f;

  Element root = VStack{
      .children = {
          Element{OuterBodyField{}}.key("field"),
      },
  };

  graph.setRoot(builder.build(root, constraints));

  ComponentKey const nestedBodyKey{
      LocalId::fromString("field"),
      LocalId::fromString("$flux.body"),
  };
  ComponentKey const firstChildKey{
      LocalId::fromString("field"),
      LocalId::fromIndex(0),
  };

  std::optional<Rect> nestedBodyRect = graph.rectForKey(nestedBodyKey);
  std::optional<Rect> firstChildRect = graph.rectForKey(firstChildKey);
  REQUIRE(nestedBodyRect.has_value());
  REQUIRE(firstChildRect.has_value());
  CHECK(nestedBodyRect->width == doctest::Approx(120.f));
  CHECK(nestedBodyRect->height == doctest::Approx(32.f));
  CHECK(firstChildRect->width == doctest::Approx(20.f));
  CHECK(firstChildRect->height == doctest::Approx(10.f));
}

TEST_CASE("SceneGraph geometry queries use current frames and previous-frame fallback") {
  scenegraph::SceneGraph graph{};
  ComponentKey const parentKey{LocalId::fromString("parent")};
  ComponentKey const childKey{LocalId::fromString("parent"), LocalId::fromString("child")};
  ComponentKey const grandchildKey{
      LocalId::fromString("parent"),
      LocalId::fromString("child"),
      LocalId::fromString("grandchild"),
  };
  ComponentKey const removedKey{LocalId::fromString("removed")};

  graph.beginGeometryBuild();
  graph.recordGeometry(parentKey, Rect{1.f, 2.f, 30.f, 40.f});
  graph.recordGeometry(childKey, Rect{3.f, 4.f, 10.f, 12.f});
  graph.recordGeometry(removedKey, Rect{7.f, 8.f, 9.f, 10.f});
  graph.finishGeometryBuild();

  REQUIRE(graph.rectForKey(parentKey).has_value());
  CHECK(*graph.rectForKey(parentKey) == Rect{1.f, 2.f, 30.f, 40.f});

  std::optional<Rect> initialPrefixRect = graph.rectForLeafKeyPrefix(grandchildKey);
  REQUIRE(initialPrefixRect.has_value());
  CHECK(initialPrefixRect->x == doctest::Approx(3.f));
  CHECK(initialPrefixRect->y == doctest::Approx(4.f));

  graph.beginGeometryBuild();
  graph.recordGeometry(parentKey, Rect{11.f, 12.f, 30.f, 40.f});
  graph.finishGeometryBuild();

  std::optional<Rect> currentRect = graph.rectForKey(parentKey);
  REQUIRE(currentRect.has_value());
  CHECK(currentRect->x == doctest::Approx(11.f));
  CHECK(currentRect->y == doctest::Approx(12.f));

  std::optional<Rect> removedRect = graph.rectForKey(removedKey);
  REQUIRE(removedRect.has_value());
  CHECK(removedRect->x == doctest::Approx(7.f));
  CHECK(removedRect->y == doctest::Approx(8.f));

  std::optional<Rect> prefixRect = graph.rectForTapAnchor(grandchildKey);
  REQUIRE(prefixRect.has_value());
  CHECK(prefixRect->x == doctest::Approx(3.f));
  CHECK(prefixRect->y == doctest::Approx(4.f));
}
