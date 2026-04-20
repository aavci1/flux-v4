#include "SceneBuilderTestSupport.hpp"

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
  CHECK(tree->children()[1]->position.y == doctest::Approx(20.f));
  CHECK(parentCalls > initialParentCalls);
  CHECK(childCalls == initialChildCalls);
  CHECK(textSystem.layoutCount == initialLayoutCount);

  std::optional<Rect> afterRect = geometry.forKey(ComponentKey{LocalId::fromString("child")});
  REQUIRE(afterRect.has_value());
  CHECK(afterRect->x == doctest::Approx(beforeRect->x));
  CHECK(afterRect->y > beforeRect->y);
}

TEST_CASE("SceneBuilder: retained composite body rebuilds when an environment dependency changes") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 120.f;

  Signal<bool> darkSignal{false};
  State<bool> dark{&darkSignal};
  int parentCalls = 0;
  int childCalls = 0;

  auto makeRoot = [&]() -> Element {
    return Element{ThemeSensitiveParent{
        .dark = dark,
        .parentCalls = &parentCalls,
        .childCalls = &childCalls,
    }};
  };

  scope.store.beginRebuild(true);
  std::unique_ptr<SceneNode> tree = builder.build(makeRoot(), NodeId{1ull}, constraints);
  scope.store.endRebuild();

  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);
  auto* firstChild = dynamic_cast<RectSceneNode*>(tree->children()[1].get());
  REQUIRE(firstChild != nullptr);
  Color firstFill{};
  REQUIRE(firstChild->fill.solidColor(&firstFill));
  CHECK(firstFill == Theme::light().separatorColor);
  CHECK(childCalls == 1);

  dark = true;

  scope.store.beginRebuild(false);
  tree = builder.build(makeRoot(), NodeId{1ull}, constraints, std::move(tree));
  scope.store.endRebuild();

  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);
  auto* secondChild = dynamic_cast<RectSceneNode*>(tree->children()[1].get());
  REQUIRE(secondChild != nullptr);
  Color secondFill{};
  REQUIRE(secondChild->fill.solidColor(&secondFill));
  CHECK(secondFill == Theme::dark().separatorColor);
  CHECK(childCalls == 2);
}
