#include "SceneBuilderTestSupport.hpp"

using namespace std::chrono_literals;

TEST_CASE("SceneBuilder preserves scroll offset through stateful rebuilds without node reuse") {
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

  scope.store.beginRebuild(true);
  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(makeScrollElement(), constraints);
  scope.store.endRebuild();
  auto* viewport = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(viewport != nullptr);
  REQUIRE(viewport->interaction() != nullptr);
  REQUIRE(viewport->interaction()->onScroll);
  REQUIRE(viewport->children().size() == 2);

  auto* indicatorOverlay = dynamic_cast<scenegraph::RectNode*>(viewport->children()[1].get());
  REQUIRE(indicatorOverlay != nullptr);
  CHECK(indicatorOverlay->opacity() == doctest::Approx(0.f));

  viewport->interaction()->onScroll(Vec2{0.f, -12.f});
  scope.store.beginRebuild(false);
  tree = builder.build(makeScrollElement(), constraints);
  scope.store.endRebuild();
  viewport = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(viewport != nullptr);
  auto* contentGroup = viewport->children()[0].get();
  CHECK(contentGroup->position().y == doctest::Approx(-12.f));
  REQUIRE(contentGroup->children().size() == 2);
  CHECK(contentGroup->children()[0]->position().y == doctest::Approx(0.f));
  CHECK(contentGroup->children()[1]->position().y == doctest::Approx(30.f));

  indicatorOverlay = dynamic_cast<scenegraph::RectNode*>(viewport->children()[1].get());
  REQUIRE(indicatorOverlay != nullptr);
  CHECK(indicatorOverlay->opacity() == doctest::Approx(1.f));
}

TEST_CASE("SceneBuilder composite root scroll view keeps local state separate from outer body") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  scope.store.beginRebuild(true);
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 80.f;
  constraints.maxHeight = 40.f;

  Element root = CompositeRootScrollView{};
  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(root, constraints);
  scope.store.endRebuild();
  auto* viewport = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(viewport != nullptr);
  REQUIRE(viewport->interaction() != nullptr);
  REQUIRE(viewport->interaction()->onScroll);

  viewport->interaction()->onScroll(Vec2{0.f, -12.f});
  scope.store.beginRebuild(false);
  tree = builder.build(root, constraints);
  scope.store.endRebuild();
  viewport = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(viewport != nullptr);
  auto* contentGroup = viewport->children()[0].get();
  CHECK(contentGroup->position().y == doctest::Approx(-12.f));
  REQUIRE(contentGroup->children().size() == 2);
  CHECK(contentGroup->children()[0]->position().y == doctest::Approx(0.f));
  CHECK(contentGroup->children()[1]->position().y == doctest::Approx(30.f));
}

TEST_CASE("SceneBuilder geometry survives rebuilt trees because it lives on SceneGraph") {
  NullTextSystem textSystem{};
  scenegraph::SceneGraph graph{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &graph};

  LayoutConstraints constraints{};
  constraints.maxWidth = 160.f;
  constraints.maxHeight = 80.f;

  Element root = HStack{
      .spacing = 8.f,
      .children = {
          keyedRect("left", 40.f, 24.f),
          keyedRect("right", 60.f, 24.f),
      },
  };

  graph.setRoot(builder.build(root, constraints));
  std::optional<Rect> leftRect = graph.rectForKey(ComponentKey{LocalId::fromString("left")});
  std::optional<Rect> rightRect = graph.rectForKey(ComponentKey{LocalId::fromString("right")});
  REQUIRE(leftRect.has_value());
  REQUIRE(rightRect.has_value());

  graph.setRoot(builder.build(root, constraints));
  REQUIRE(graph.rectForKey(ComponentKey{LocalId::fromString("left")}).has_value());
  REQUIRE(graph.rectForKey(ComponentKey{LocalId::fromString("right")}).has_value());
}
