#include "SceneBuilderTestSupport.hpp"

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
  CHECK(aRect->x == doctest::Approx(50.f));
  CHECK(aRect->y == doctest::Approx(43.5f));
  CHECK(aRect->width == doctest::Approx(20.f));
  CHECK(aRect->height == doctest::Approx(10.f));
  CHECK(bRect->x == doctest::Approx(45.f));
  CHECK(bRect->y == doctest::Approx(61.5f));
  CHECK(bRect->width == doctest::Approx(30.f));
  CHECK(bRect->height == doctest::Approx(15.f));
}

TEST_CASE("SceneBuilder: geometry index tracks centered VStack child cross-axis offsets") {
  NullTextSystem textSystem{};
  SceneGeometryIndex geometry{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &geometry};

  LayoutConstraints constraints{};
  constraints.maxWidth = 100.f;
  constraints.maxHeight = 60.f;

  Element centeredColumn = VStack{
      .alignment = Alignment::Center,
      .children = {
          keyedRect("column-child", 20.f, 10.f),
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(centeredColumn, NodeId{1ull}, constraints);
  REQUIRE(tree);

  std::optional<Rect> columnChildRect =
      geometry.rectForKey(ComponentKey{LocalId::fromString("column-child")});
  REQUIRE(columnChildRect.has_value());
  CHECK(columnChildRect->x == doctest::Approx(40.f));
  CHECK(columnChildRect->y == doctest::Approx(25.f));
  CHECK(columnChildRect->width == doctest::Approx(20.f));
  CHECK(columnChildRect->height == doctest::Approx(10.f));

}

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
        .children = {
            demoColorBlock(20.f, 10.f),
            demoColorBlock(12.f, 12.f),
        },
    }
        .padding(10.f, 14.f, 10.f, 14.f)
        .stroke(StrokeStyle::solid(Color::primary(), 1.f))
        .cornerRadius(CornerRadius{4.f});
  }
};

struct OuterBodyField {
  Element body() const {
    return Element{InnerBodyField{}};
  }
};

} // namespace

TEST_CASE("SceneBuilder: geometry index records outer layout rect for body-expanded components") {
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
          demoColorBlock(120.f, 20.f),
          Element{DecoratedBodyComponent{}}.key("field"),
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(root, NodeId{1ull}, constraints);
  REQUIRE(tree);

  std::optional<Rect> fieldRect = geometry.rectForKey(ComponentKey{LocalId::fromString("field")});
  REQUIRE(fieldRect.has_value());
  CHECK(fieldRect->x == doctest::Approx(26.f));
  CHECK(fieldRect->y == doctest::Approx(28.f));
  CHECK(fieldRect->width == doctest::Approx(68.f));
  CHECK(fieldRect->height == doctest::Approx(24.f));
}

TEST_CASE("SceneBuilder: nested body wrapper geometry does not collide with first scene child") {
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
      .children = {
          Element{OuterBodyField{}}.key("field"),
      },
  };

  std::unique_ptr<SceneNode> tree = builder.build(root, NodeId{1ull}, constraints);
  REQUIRE(tree);

  ComponentKey const nestedBodyKey{
      LocalId::fromString("field"),
      LocalId::fromString("$flux.body"),
  };
  ComponentKey const firstChildKey{
      LocalId::fromString("field"),
      LocalId::fromIndex(0),
  };

  std::optional<Rect> nestedBodyRect = geometry.rectForKey(nestedBodyKey);
  std::optional<Rect> firstChildRect = geometry.rectForKey(firstChildKey);
  REQUIRE(nestedBodyRect.has_value());
  REQUIRE(firstChildRect.has_value());
  CHECK(nestedBodyRect->width == doctest::Approx(120.f));
  CHECK(nestedBodyRect->height == doctest::Approx(32.f));
  CHECK(firstChildRect->width == doctest::Approx(20.f));
  CHECK(firstChildRect->height == doctest::Approx(10.f));
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
