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
  CHECK(aRect->x == doctest::Approx(0.f));
  CHECK(aRect->y == doctest::Approx(43.5f));
  CHECK(aRect->width == doctest::Approx(20.f));
  CHECK(aRect->height == doctest::Approx(10.f));
  CHECK(bRect->x == doctest::Approx(0.f));
  CHECK(bRect->y == doctest::Approx(61.5f));
  CHECK(bRect->width == doctest::Approx(30.f));
  CHECK(bRect->height == doctest::Approx(15.f));
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
