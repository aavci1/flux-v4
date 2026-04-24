#include "SceneBuilderTestSupport.hpp"

#include <Flux/UI/Views/PopoverCalloutShape.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>

TEST_CASE("SceneBuilder keeps centered text in its assigned box") {
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

  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(text, constraints);
  auto* textNode = dynamic_cast<scenegraph::TextNode*>(tree.get());
  REQUIRE(textNode != nullptr);
  CHECK(textNode->bounds().width == doctest::Approx(320.f));
  CHECK(textNode->bounds().height == doctest::Approx(320.f));
}

TEST_CASE("SceneBuilder wraps padded text in a RectNode envelope and sizes the inner text box") {
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

  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(text, constraints);
  auto* wrapper = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(wrapper != nullptr);
  REQUIRE(wrapper->children().size() == 1);
  CHECK(wrapper->bounds().width == doctest::Approx(200.f));
  CHECK(wrapper->bounds().height == doctest::Approx(80.f));

  auto* textNode = dynamic_cast<scenegraph::TextNode*>(wrapper->children()[0].get());
  REQUIRE(textNode != nullptr);
  CHECK(textNode->position().x == doctest::Approx(12.f));
  CHECK(textNode->position().y == doctest::Approx(8.f));
  CHECK(textNode->bounds().width == doctest::Approx(176.f));
  CHECK(textNode->bounds().height == doctest::Approx(64.f));
}

TEST_CASE("SceneBuilder modifier envelope owns the outer hit area and interaction") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 180.f;
  constraints.maxHeight = 49.f;

  bool tapped = false;
  Element buttonLike = Text{
      .text = "Create Invoice",
      .horizontalAlignment = HorizontalAlignment::Center,
      .verticalAlignment = VerticalAlignment::Center,
  }
                           .padding(16.f)
                           .fill(FillStyle::solid(Colors::black))
                           .cursor(Cursor::Hand)
                           .onTap([&] { tapped = true; });

  scenegraph::SceneGraph graph{builder.build(buttonLike, constraints)};
  auto* wrapper = dynamic_cast<scenegraph::RectNode*>(&graph.root());
  REQUIRE(wrapper != nullptr);
  REQUIRE(wrapper->interaction() != nullptr);
  CHECK(wrapper->interaction()->cursor == Cursor::Hand);
  CHECK(wrapper->bounds().width == doctest::Approx(180.f));
  CHECK(wrapper->bounds().height == doctest::Approx(49.f));

  auto hit = scenegraph::hitTestInteraction(
      graph, Point{wrapper->bounds().width - 1.f, wrapper->bounds().height * 0.5f});
  REQUIRE(hit.has_value());
  CHECK(hit->node == &graph.root());
  REQUIRE(hit->interaction->onTap);
  hit->interaction->onTap();
  CHECK(tapped);
}

TEST_CASE("SceneBuilder keeps composite root modifier shell interaction and chrome on a RectNode") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 240.f;
  constraints.maxHeight = 64.f;

  bool tapped = false;
  std::unique_ptr<scenegraph::SceneNode> tree =
      builder.build(Element{CompositeRootModifierShell{.tapped = &tapped}}, constraints);
  REQUIRE(tree != nullptr);

  auto* wrapper = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(wrapper != nullptr);
  REQUIRE(wrapper->interaction() != nullptr);
  CHECK(wrapper->interaction()->cursor == Cursor::Hand);
  CHECK(wrapper->interaction()->focusable);
  CHECK(wrapper->bounds().width > 0.f);
  CHECK(wrapper->bounds().height > 0.f);

  scenegraph::SceneGraph graph{std::move(tree)};
  auto hit = scenegraph::hitTestInteraction(graph, Point{graph.root().bounds().width - 1.f,
                                                         graph.root().bounds().height * 0.5f});
  REQUIRE(hit.has_value());
  REQUIRE(hit->interaction->onTap);
  hit->interaction->onTap();
  CHECK(tapped);
}

TEST_CASE("SceneBuilder preserves outer explicit size around composite body chrome") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 400.f;
  constraints.maxHeight = 80.f;

  Element el = Element{CompositeRootModifierShell{}}.size(240.f, 64.f);

  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(el, constraints);
  REQUIRE(tree != nullptr);
  CHECK(tree->kind() == scenegraph::SceneNodeKind::Rect);
  CHECK(tree->bounds().width == doctest::Approx(240.f));
  CHECK(tree->bounds().height == doctest::Approx(64.f));
  REQUIRE(tree->children().size() == 1);
  auto* inner = dynamic_cast<scenegraph::RectNode*>(tree->children()[0].get());
  REQUIRE(inner != nullptr);
  CHECK(inner->bounds().width > 0.f);
  CHECK(inner->bounds().height > 0.f);
}

namespace {

struct NestedFocusableInnerBody {
  Element body() const {
    return Text{.text = "Focusable"}
        .padding(8.f, 12.f, 8.f, 12.f)
        .fill(FillStyle::solid(Colors::black))
        .focusable(true)
        .onTap([] {});
  }
};

struct NestedFocusableOuterBody {
  Element body() const { return Element{NestedFocusableInnerBody{}}; }
};

} // namespace

TEST_CASE("SceneBuilder keeps nested body focus targets inside the inner component subtree") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 240.f;
  constraints.maxHeight = 64.f;

  Element rootElement = Element{NestedFocusableOuterBody{}}.key("field");
  scenegraph::SceneGraph graph{
      builder.build(rootElement, constraints, ComponentKey{LocalId::fromString("field")})};
  ComponentKey const expectedKey{
      LocalId::fromString("field"),
      LocalId::fromString("$flux.body"),
  };
  auto isPrefix = [](ComponentKey const& prefix, ComponentKey const& key) {
    return prefix.size() <= key.size() && std::equal(prefix.begin(), prefix.end(), key.begin());
  };
  ComponentKey const outerKey{LocalId::fromString("field")};

  std::vector<ComponentKey> const focusable = scenegraph::collectFocusableKeys(graph);
  REQUIRE(focusable.size() == 1);
  CHECK(isPrefix(expectedKey, focusable.front()));
  CHECK(focusable.front() != outerKey);

  auto hit = scenegraph::hitTestInteraction(graph, Point{16.f, 16.f});
  REQUIRE(hit.has_value());
  CHECK(isPrefix(expectedKey, hit->interaction->stableTargetKey));
  CHECK(hit->interaction->stableTargetKey != outerKey);
}

TEST_CASE("SceneBuilder records composite geometry from the outer modifier shell when it differs from body content") {
  NullTextSystem textSystem{};
  scenegraph::SceneGraph graph{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current(), &graph};

  LayoutConstraints constraints{};
  constraints.maxWidth = 240.f;
  constraints.maxHeight = 80.f;

  Element el = VStack{
      .alignment = Alignment::Start,
      .children = children(
          Element{CompositeRootModifierShell{}}.key("shell").size(240.f, 64.f)
      ),
  };

  graph.setRoot(builder.build(el, constraints));
  std::optional<Rect> shellRect = graph.rectForKey(ComponentKey{LocalId::fromString("shell")});
  REQUIRE(shellRect.has_value());
  CHECK(shellRect->x == doctest::Approx(0.f));
  CHECK(shellRect->y == doctest::Approx(0.f));
  CHECK(shellRect->width == doctest::Approx(240.f));
  CHECK(shellRect->height == doctest::Approx(64.f));
}

TEST_CASE("SceneBuilder emits clipped scroll viewport and translated content group") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 80.f;
  constraints.maxHeight = 40.f;

  Element scroll = ScrollView{
      .axis = ScrollAxis::Vertical,
      .children = {
          keyedRect("a", 60.f, 30.f),
          keyedRect("b", 60.f, 30.f),
      },
  };

  scope.store.beginRebuild(true);
  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(scroll, constraints);
  scope.store.endRebuild();
  auto* viewport = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(viewport != nullptr);
  CHECK(viewport->clipsContents());
  REQUIRE(viewport->children().size() == 2);
  CHECK(viewport->bounds().width == doctest::Approx(80.f));
  CHECK(viewport->bounds().height == doctest::Approx(40.f));

  auto* contentGroup = dynamic_cast<scenegraph::SceneNode*>(viewport->children()[0].get());
  REQUIRE(contentGroup != nullptr);
  CHECK(contentGroup->position().y == doctest::Approx(0.f));
  REQUIRE(contentGroup->children().size() == 2);
  CHECK(contentGroup->children()[0]->position().y == doctest::Approx(0.f));
  CHECK(contentGroup->children()[1]->position().y == doctest::Approx(30.f));

  REQUIRE(viewport->interaction() != nullptr);
  REQUIRE(viewport->interaction()->onScroll);
  viewport->interaction()->onScroll(Vec2{0.f, -12.f});

  scope.store.beginRebuild(false);
  tree = builder.build(scroll, constraints);
  scope.store.endRebuild();
  viewport = dynamic_cast<scenegraph::RectNode*>(tree.get());
  REQUIRE(viewport != nullptr);
  auto* scrolledContent = viewport->children()[0].get();
  CHECK(scrolledContent->position().y == doctest::Approx(-12.f));
  REQUIRE(scrolledContent->children().size() == 2);
  CHECK(scrolledContent->children()[0]->position().y == doctest::Approx(0.f));
  CHECK(scrolledContent->children()[1]->position().y == doctest::Approx(30.f));

  auto* indicatorOverlay = dynamic_cast<scenegraph::RectNode*>(viewport->children()[1].get());
  REQUIRE(indicatorOverlay != nullptr);
  CHECK(indicatorOverlay->opacity() == doctest::Approx(1.f));
}

TEST_CASE("SceneBuilder uses per-node transforms for ScaleAroundCenter") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 120.f;
  constraints.maxHeight = 80.f;

  Element scaled = Element{ScaleAroundCenter{
      .scale = 1.25f,
      .child = Element{Rectangle{}}.size(40.f, 20.f),
  }};

  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(scaled, constraints);
  REQUIRE(tree != nullptr);
  CHECK(tree->kind() == scenegraph::SceneNodeKind::Group);
  CHECK_FALSE(tree->transform().isTranslationOnly());
  REQUIRE(tree->children().size() == 1);
  CHECK(tree->bounds().width == doctest::Approx(50.f));
  CHECK(tree->bounds().height == doctest::Approx(25.f));
}

TEST_CASE("SceneBuilder emits popover chrome as a PathNode with content sibling") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 220.f;
  constraints.maxHeight = 140.f;

  Element popover = Element{PopoverCalloutShape{
      .placement = PopoverPlacement::Below,
      .content = Element{Rectangle{}}.size(80.f, 40.f),
  }};

  std::unique_ptr<scenegraph::SceneNode> tree = builder.build(popover, constraints);
  REQUIRE(tree != nullptr);
  CHECK(tree->kind() == scenegraph::SceneNodeKind::Group);
  REQUIRE(tree->children().size() == 2);
  auto* chrome = dynamic_cast<scenegraph::PathNode*>(tree->children()[0].get());
  auto* content = dynamic_cast<scenegraph::RectNode*>(tree->children()[1].get());
  REQUIRE(chrome != nullptr);
  REQUIRE(content != nullptr);
  CHECK(chrome->bounds().width >= content->bounds().width);
  CHECK(chrome->bounds().height >= content->bounds().height);
}
