#include "SceneBuilderTestSupport.hpp"

using namespace std::chrono_literals;

namespace {

struct StatefulIncrementalCard {
  float width = 60.f;

  Element body() const {
    auto state = useState(0);
    (void)state;
    return Element{Rectangle{}}.size(width, 30.f);
  }
};

struct StableSkipPanel {
  Element body() const {
    return VStack{
        .children = {
            Element{Rectangle{}}.size(20.f, 10.f),
            Element{Rectangle{}}.size(20.f, 10.f),
        },
    };
  }
};

struct CallbackSkipPanel {
  int* observed = nullptr;
  int value = 0;

  Element body() const {
    return Element{Rectangle{}}.size(20.f, 10.f).onTap([observed = observed, value = value] {
      if (observed) {
        *observed = value;
      }
    });
  }
};

struct ScrollReusableRow {
  float height = 20.f;

  Element body() const {
    return Element{Rectangle{}}.size(60.f, height);
  }
};

} // namespace

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

TEST_CASE("ScrollView reuses stable content rows on offset-only rebuild") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 80.f;
  constraints.maxHeight = 40.f;
  ComponentKey const rootKey{LocalId::fromString("scroll")};

  auto makeScrollElement = []() -> Element {
    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = {
            Element{ScrollReusableRow{20.f}}.key("a"),
            Element{ScrollReusableRow{20.f}}.key("b"),
            Element{ScrollReusableRow{20.f}}.key("c"),
        },
    };
  };

  scope.store.beginRebuild(true);
  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(makeScrollElement(), constraints, rootKey);
  scope.store.endRebuild();
  auto* viewport = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(viewport != nullptr);
  REQUIRE(viewport->interaction() != nullptr);
  REQUIRE(viewport->interaction()->onScroll);
  viewport->interaction()->onScroll(Vec2{0.f, -12.f});

  scope.store.beginRebuild(false);
  tree = builder.buildSubtree(makeScrollElement(), constraints, LayoutHints{}, Point{}, rootKey, Size{},
                              false, false, Point{}, std::move(tree));
  scope.store.endRebuild();

  CHECK(builder.lastBuildStats().skippedSubtrees >= 3);
  viewport = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(viewport != nullptr);
  REQUIRE(viewport->children().size() >= 1);
  auto* contentGroup = viewport->children()[0].get();
  CHECK(contentGroup->position().y == doctest::Approx(-12.f));
  REQUIRE(contentGroup->children().size() == 3);
  CHECK(contentGroup->children()[0]->position().y == doctest::Approx(0.f));
  CHECK(contentGroup->children()[1]->position().y == doctest::Approx(20.f));
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

TEST_CASE("SceneBuilder skips stable retained composite subtrees") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 100.f;
  constraints.maxHeight = 100.f;
  ComponentKey const rootKey{LocalId::fromString("root")};

  Element root = StableSkipPanel{};
  scope.store.beginRebuild(true);
  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(root, constraints, rootKey);
  scope.store.endRebuild();
  scenegraph::SceneNode* original = tree.get();

  scope.store.markCompositeDirty(rootKey);
  scope.store.beginRebuild(false);
  tree = builder.buildSubtree(root, constraints, LayoutHints{}, Point{}, rootKey, Size{}, false,
                              false, Point{}, std::move(tree));
  scope.store.endRebuild();

  CHECK(tree.get() == original);
  CHECK(builder.lastBuildStats().skippedSubtrees == 1);
  CHECK(builder.lastBuildStats().materializedNodes == 0);
}

TEST_CASE("SceneBuilder skipped subtrees invoke current interaction closures") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 100.f;
  constraints.maxHeight = 100.f;
  ComponentKey const rootKey{LocalId::fromString("root")};
  int observed = 0;

  Element first = CallbackSkipPanel{.observed = &observed, .value = 1};
  scope.store.beginRebuild(true);
  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(first, constraints, rootKey);
  scope.store.endRebuild();
  scenegraph::RectNode* rect = findNode<scenegraph::RectNode>(tree.get());
  REQUIRE(rect != nullptr);
  REQUIRE(rect->interaction() != nullptr);
  REQUIRE(rect->interaction()->onTap);
  rect->interaction()->onTap();
  CHECK(observed == 1);
  scenegraph::SceneNode* original = tree.get();

  Element second = CallbackSkipPanel{.observed = &observed, .value = 2};
  scope.store.markCompositeDirty(rootKey);
  scope.store.beginRebuild(false);
  tree = builder.buildSubtree(second, constraints, LayoutHints{}, Point{}, rootKey, Size{}, false,
                              false, Point{}, std::move(tree));
  scope.store.endRebuild();

  CHECK(tree.get() == original);
  CHECK(builder.lastBuildStats().skippedSubtrees == 1);
  rect = findNode<scenegraph::RectNode>(tree.get());
  REQUIRE(rect != nullptr);
  REQUIRE(rect->interaction() != nullptr);
  REQUIRE(rect->interaction()->onTap);
  rect->interaction()->onTap();
  CHECK(observed == 2);
}

TEST_CASE("SceneBuilder subtree rebuild preserves retained parent slot offset") {
  NullTextSystem textSystem{};
  scenegraph::SceneGraph graph{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &graph};

  LayoutConstraints constraints{};
  constraints.maxWidth = 160.f;
  constraints.maxHeight = 120.f;

  ComponentKey const childKey{LocalId::fromString("bottom")};
  Element root = VStack{
      .spacing = 8.f,
      .children = {
          keyedRect("top", 72.f, 20.f),
          Element{StatefulIncrementalCard{72.f}}.key("bottom"),
      },
  };

  scope.store.beginRebuild(true);
  graph.setRoot(builder.build(root, constraints));
  scope.store.endRebuild();

  std::optional<ComponentBuildSnapshot> const childSnapshot = scope.store.buildSnapshot(childKey);
  Element const* sceneElement = scope.store.sceneElement(childKey);
  std::optional<Rect> const previousRect = graph.rectForKey(childKey);
  scenegraph::SceneNode* originalNode = graph.nodeForKey(childKey);
  REQUIRE(childSnapshot.has_value());
  REQUIRE(sceneElement != nullptr);
  REQUIRE(previousRect.has_value());
  REQUIRE(originalNode != nullptr);

  Point const retainedRootOffset{
      originalNode->position().x - (previousRect->x - childSnapshot->origin.x),
      originalNode->position().y - (previousRect->y - childSnapshot->origin.y),
  };
  Point const originalPosition = originalNode->position();
  CHECK(originalPosition == retainedRootOffset);

  scope.store.markCompositeDirty(childKey);
  scope.store.beginRebuild(false);

  scenegraph::SceneGraph patchGraph{};
  SceneBuilder patchBuilder{textSystem, EnvironmentStack::current(), &patchGraph};
  std::unique_ptr<scenegraph::SceneNode> existing =
      graph.replaceNodeForKey(childKey, std::make_unique<scenegraph::GroupNode>());
  REQUIRE(existing != nullptr);

  std::unique_ptr<scenegraph::SceneNode> replacement =
      patchBuilder.buildSubtree(*sceneElement, childSnapshot->constraints, childSnapshot->hints,
                                childSnapshot->origin, childKey, childSnapshot->assignedSize,
                                childSnapshot->hasAssignedWidth, childSnapshot->hasAssignedHeight,
                                retainedRootOffset, std::move(existing));
  REQUIRE(replacement != nullptr);
  CHECK(replacement->position() == originalPosition);

  std::unique_ptr<scenegraph::SceneNode> removed =
      graph.replaceNodeForKey(childKey, std::move(replacement));
  REQUIRE(removed != nullptr);
  graph.replaceSubtreeData(childKey, patchGraph);
  scope.store.markComponentsOutsideSubtreeVisited(childKey);
  scope.store.endRebuild();

  scenegraph::SceneNode* rebuiltNode = graph.nodeForKey(childKey);
  REQUIRE(rebuiltNode != nullptr);
  CHECK(rebuiltNode->position() == originalPosition);
}
