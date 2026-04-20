#include "SceneBuilderTestSupport.hpp"

#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Popover.hpp>
#include <Flux/UI/Views/Grid.hpp>

namespace {

struct BoundsDrivenComposite {
  float fallbackHeight = 0.f;

  Element body() const {
    Rect const bounds = useBounds();
    return Element{Rectangle{}}.size(bounds.width > 0.f ? bounds.width : 1.f,
                                     bounds.height > 0.f ? bounds.height : fallbackHeight);
  }
};

struct CompositeFooterPanel {
  Element body() const {
    return VStack{
        .spacing = 0.f,
        .alignment = Alignment::Stretch,
        .children = children(
            Element{Rectangle{}}.size(40.f, 30.f),
            Element{Rectangle{}}.size(40.f, 20.f)),
    };
  }
};

} // namespace

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

TEST_CASE("SceneBuilder: icon reserves a square centered allocation matching its requested size") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 200.f;

  std::unique_ptr<SceneNode> tree =
      builder.build(Element{Icon{.name = IconName::Check, .size = 16.f}}, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);

  auto findText = [&](this auto const& self, SceneNode* node) -> TextSceneNode* {
    if (!node) {
      return nullptr;
    }
    if (auto* text = dynamic_cast<TextSceneNode*>(node)) {
      return text;
    }
    for (std::unique_ptr<SceneNode> const& child : node->children()) {
      if (TextSceneNode* text = self(child.get())) {
        return text;
      }
    }
    return nullptr;
  };

  TextSceneNode* textNode = findText(tree.get());
  REQUIRE(textNode != nullptr);
  CHECK(textNode->allocation.width == doctest::Approx(16.f));
  CHECK(textNode->allocation.height == doctest::Approx(16.f));
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
  CHECK(wrapper->chromeRect.width == doctest::Approx(200.f));
  CHECK(wrapper->chromeRect.height == doctest::Approx(80.f));

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

TEST_CASE("SceneBuilder: modifier wrapper hit area follows the outer chrome box") {
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

  std::unique_ptr<SceneNode> tree = builder.build(buttonLike, NodeId{1ull}, constraints);
  auto* wrapper = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(wrapper != nullptr);
  REQUIRE(wrapper->interaction() != nullptr);
  CHECK(wrapper->interaction()->cursor == Cursor::Hand);
  CHECK(wrapper->chromeRect.width == doctest::Approx(180.f));
  CHECK(wrapper->chromeRect.height == doctest::Approx(49.f));

  SceneNode* hit = tree->hitTest(Point{179.f, 24.f});
  REQUIRE(hit == tree.get());
  REQUIRE(tree->interaction()->onTap);
  tree->interaction()->onTap();
  CHECK(tapped);
}

TEST_CASE("SceneBuilder: composite root keeps body modifier shell interaction and chrome") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 240.f;
  constraints.maxHeight = 64.f;

  bool tapped = false;
  std::unique_ptr<SceneNode> tree =
      builder.build(Element{CompositeRootModifierShell{.tapped = &tapped}}, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);

  auto* wrapper = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(wrapper != nullptr);
  REQUIRE(wrapper->interaction() != nullptr);
  CHECK(wrapper->interaction()->cursor == Cursor::Hand);
  CHECK(wrapper->interaction()->focusable);
  CHECK(wrapper->chromeRect.width > 0.f);
  CHECK(wrapper->chromeRect.height > 0.f);
  CHECK(wrapper->bounds.width >= wrapper->chromeRect.width);
  CHECK(wrapper->bounds.height >= wrapper->chromeRect.height);

  SceneNode* hit = tree->hitTest(Point{wrapper->chromeRect.width - 1.f, wrapper->chromeRect.height * 0.5f});
  REQUIRE(hit == tree.get());
  REQUIRE(tree->interaction()->onTap);
  tree->interaction()->onTap();
  CHECK(tapped);
}

TEST_CASE("SceneBuilder: composite root preserves outer size modifiers around body chrome") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 400.f;
  constraints.maxHeight = 80.f;

  Element el = Element{CompositeRootModifierShell{}}
                   .size(240.f, 64.f);

  std::unique_ptr<SceneNode> tree = builder.build(el, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  CHECK(tree->kind() == SceneNodeKind::Group);
  CHECK(tree->bounds.width == doctest::Approx(240.f));
  CHECK(tree->bounds.height == doctest::Approx(64.f));
  REQUIRE(tree->children().size() == 1);
  auto* inner = dynamic_cast<ModifierSceneNode*>(tree->children()[0].get());
  REQUIRE(inner != nullptr);
  CHECK(inner->chromeRect.width > 0.f);
  CHECK(inner->chromeRect.height > 0.f);
}

TEST_CASE("SceneBuilder: sibling composites with distinct props keep distinct bodies") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 500.f;
  constraints.maxHeight = 64.f;

  StateStore store;
  StateStore::setCurrent(&store);
  store.beginRebuild(true);
  Element treeEl = HStack{
      .spacing = 8.f,
      .children = children(
          Element{LabeledComposite{.label = "Back"}},
          Element{LabeledComposite{.label = "Comments"}},
          Element{LabeledComposite{.label = "History"}}),
  };
  std::unique_ptr<SceneNode> tree = builder.build(treeEl, NodeId{1ull}, constraints);
  store.endRebuild();
  StateStore::setCurrent(nullptr);

  REQUIRE(tree != nullptr);
  std::vector<std::string> labels;
  collectTextLabels(*tree, labels);
  REQUIRE(labels.size() == 3);
  CHECK(labels[0] == "Back");
  CHECK(labels[1] == "Comments");
  CHECK(labels[2] == "History");
}

TEST_CASE("SceneBuilder: constraint-sensitive composite bodies rebuild when a flex slot becomes assigned") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 320.f;
  constraints.maxHeight = 240.f;

  StoreScope storeScope;
  storeScope.store.beginRebuild(true);
  std::unique_ptr<SceneNode> tree = builder.build(
      Element{VStack{
          .spacing = 12.f,
          .children = children(
              Text{.text = "Markdown styler"},
              Element{BoundsDrivenComposite{.fallbackHeight = 32.f}}.flex(1.f, 1.f, 0.f)),
      }},
      NodeId{1ull}, constraints);
  storeScope.store.endRebuild();

  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);
  auto* rectNode = dynamic_cast<RectSceneNode*>(tree->children()[1].get());
  REQUIRE(rectNode != nullptr);
  CHECK(rectNode->size.width == doctest::Approx(320.f));
  CHECK(rectNode->size.height == doctest::Approx(214.f));
}

TEST_CASE("SceneBuilder: sibling link buttons keep distinct labels") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 900.f;
  constraints.maxHeight = 64.f;

  StateStore store;
  StateStore::setCurrent(&store);
  store.beginRebuild(true);
  Element treeEl = HStack{
      .spacing = 4.f,
      .alignment = Alignment::Center,
      .children = children(
          Text{.text = "Still waiting on approval?"},
          LinkButton{
              .label = "Mark review as approved",
              .style = LinkButton::Style{},
          },
          Text{.text = "or"},
          LinkButton{
              .label = "contact support",
              .disabled = true,
              .style = LinkButton::Style{},
          }),
  };
  std::unique_ptr<SceneNode> tree = builder.build(treeEl, NodeId{1ull}, constraints);
  store.endRebuild();
  StateStore::setCurrent(nullptr);

  REQUIRE(tree != nullptr);
  std::vector<std::string> labels;
  collectTextLabels(*tree, labels);
  REQUIRE(labels.size() == 4);
  CHECK(labels[0] == "Still waiting on approval?");
  CHECK(labels[1] == "Mark review as approved");
  CHECK(labels[2] == "or");
  CHECK(labels[3] == "contact support");
}

TEST_CASE("SceneBuilder: nested inline-links composite keeps distinct link labels") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 900.f;
  constraints.maxHeight = 200.f;

  Signal<bool> reviewSignal{false};
  State<bool> reviewPassed{&reviewSignal};

  StateStore store;
  StateStore::setCurrent(&store);
  store.beginRebuild(true);
  std::unique_ptr<SceneNode> tree = builder.build(Element{InlineLinksComposite{.reviewPassed = reviewPassed}},
                                                  NodeId{1ull}, constraints);
  store.endRebuild();
  StateStore::setCurrent(nullptr);

  REQUIRE(tree != nullptr);
  std::vector<std::string> labels;
  collectTextLabels(*tree, labels);
  REQUIRE(labels.size() == 6);
  CHECK(labels[0] == "Need copy guidance before publishing?");
  CHECK(labels[1] == "Open the editorial checklist");
  CHECK(labels[2] == "Still waiting on approval?");
  CHECK(labels[3] == "Mark review as approved");
  CHECK(labels[4] == "or");
  CHECK(labels[5] == "contact support");
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

TEST_CASE("SceneBuilder: one-child HStack in a ZStack keeps finite row width and bottom alignment") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 120.f;

  Element overlay = ZStack{
      .horizontalAlignment = Alignment::Stretch,
      .verticalAlignment = Alignment::End,
      .children = children(
          Element{Rectangle{}}.size(200.f, 120.f),
          Element{HStack{
              .children = children(
                  Element{Rectangle{}}.size(40.f, 30.f).flex(1.f)),
          }}
              .padding(10.f)),
  };

  std::unique_ptr<SceneNode> tree = builder.build(overlay, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);

  SceneNode const& footer = *tree->children()[1];
  CHECK(footer.bounds.width == doctest::Approx(200.f));
  CHECK(footer.bounds.height == doctest::Approx(50.f));
  CHECK(footer.position.x == doctest::Approx(0.f));
  CHECK(footer.position.y == doctest::Approx(70.f));
}

TEST_CASE("SceneBuilder: non-stretch HStack does not force composite children to full cross-axis height") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 120.f;

  Element overlay = ZStack{
      .horizontalAlignment = Alignment::Stretch,
      .verticalAlignment = Alignment::End,
      .children = children(
          Element{Rectangle{}}.size(200.f, 120.f),
          Element{HStack{
              .alignment = Alignment::Center,
              .children = children(
                  Element{CompositeFooterPanel{}}.flex(1.f)),
          }}
              .padding(10.f)),
  };

  StoreScope storeScope;
  storeScope.store.beginRebuild(true);
  std::unique_ptr<SceneNode> tree = builder.build(overlay, NodeId{1ull}, constraints);
  storeScope.store.endRebuild();

  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);

  SceneNode const& footer = *tree->children()[1];
  CHECK(footer.bounds.width == doctest::Approx(200.f));
  CHECK(footer.bounds.height == doctest::Approx(70.f));
  CHECK(footer.position.y == doctest::Approx(50.f));
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

TEST_CASE("SceneBuilder: centered HStack vertically centers shorter children within the row") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 120.f;

  Element row = HStack{
      .spacing = 8.f,
      .alignment = Alignment::Center,
      .children = children(
          Element{Rectangle{}}.size(22.f, 18.f),
          Element{Rectangle{}}.size(22.f, 52.f),
          Element{Rectangle{}}.size(22.f, 26.f)),
  };

  std::unique_ptr<SceneNode> tree = builder.build(row, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 3);
  CHECK(tree->bounds.height == doctest::Approx(52.f));
  CHECK(tree->children()[0]->position.y == doctest::Approx(17.f));
  CHECK(tree->children()[1]->position.y == doctest::Approx(0.f));
  CHECK(tree->children()[2]->position.y == doctest::Approx(13.f));
}

TEST_CASE("SceneBuilder: grid rows follow wrapped content height after parent-assigned rebuild slots") {
  VariableTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 100.f;
  constraints.maxHeight = 200.f;

  Element treeEl = VStack{
      .alignment = Alignment::Start,
      .children = children(
          Grid{
              .columns = 2,
              .horizontalSpacing = 8.f,
              .verticalSpacing = 6.f,
              .horizontalAlignment = Alignment::Center,
              .verticalAlignment = Alignment::Center,
              .children = children(
                  Text{.text = "Short", .wrapping = TextWrapping::Wrap},
                  Text{.text = "Short", .wrapping = TextWrapping::Wrap},
                  Text{.text = "This wraps", .wrapping = TextWrapping::Wrap},
                  Text{.text = "Short", .wrapping = TextWrapping::Wrap}),
          }),
  };

  std::unique_ptr<SceneNode> tree = builder.build(treeEl, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 1);

  SceneNode const& grid = *tree->children()[0];
  REQUIRE(grid.children().size() == 4);
  CHECK(grid.bounds.height == doctest::Approx(48.f));
  CHECK(grid.children()[0]->position.y == doctest::Approx(0.f));
  CHECK(grid.children()[2]->position.y == doctest::Approx(20.f));
  CHECK(grid.children()[2]->bounds.height == doctest::Approx(28.f));
}

TEST_CASE("SceneBuilder: centered ZStack overlay shrink-wraps intrinsic children before centering") {
  VariableTextSystem textSystem{};
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
  CHECK(overlayColumn->position.x == doctest::Approx(5.f));
  CHECK(overlayColumn->position.y == doctest::Approx(74.f));
  CHECK(overlayColumn->bounds.width == doctest::Approx(210.f));
  CHECK(overlayColumn->bounds.height == doctest::Approx(32.f));
  REQUIRE(overlayColumn->children().size() == 2);
  CHECK(overlayColumn->children()[0]->position.x == doctest::Approx(52.5f));
  CHECK(overlayColumn->children()[0]->bounds.width == doctest::Approx(105.f));
  CHECK(overlayColumn->children()[1]->position.x == doctest::Approx(0.f));
  CHECK(overlayColumn->children()[1]->bounds.width == doctest::Approx(210.f));
  CHECK(overlayColumn->children()[1]->position.y == doctest::Approx(18.f));
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

TEST_CASE("SceneBuilder: HStack clamps child measure constraints after flex shrink under fixed root width") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.minWidth = 300.f;
  constraints.maxWidth = 300.f;
  constraints.maxHeight = 40.f;

  Element row = HStack{
      .spacing = 10.f,
      .children = children(
          Element{Rectangle{}}.size(200.f, 20.f).flex(1.f, 1.f, 0.f),
          Element{Rectangle{}}.size(200.f, 20.f).flex(1.f, 1.f, 0.f)),
  };

  std::unique_ptr<SceneNode> tree = builder.build(row, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);
  auto const* left = dynamic_cast<RectSceneNode const*>(tree->children()[0].get());
  auto const* right = dynamic_cast<RectSceneNode const*>(tree->children()[1].get());
  REQUIRE(left != nullptr);
  REQUIRE(right != nullptr);
  CHECK(left->size.width == doctest::Approx(145.f));
  CHECK(right->size.width == doctest::Approx(145.f));
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
  CHECK(overlayColumn->position.x == doctest::Approx(14.f));
  CHECK(overlayColumn->position.y == doctest::Approx(74.f));
  CHECK(overlayColumn->bounds.width == doctest::Approx(210.f));
  CHECK(overlayColumn->bounds.height == doctest::Approx(32.f));
  REQUIRE(overlayColumn->children().size() == 2);
  CHECK(overlayColumn->children()[0]->position.x == doctest::Approx(52.5f));
  CHECK(overlayColumn->children()[0]->bounds.width == doctest::Approx(105.f));
  CHECK(overlayColumn->children()[1]->position.x == doctest::Approx(0.f));
  CHECK(overlayColumn->children()[1]->bounds.width == doctest::Approx(210.f));
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

TEST_CASE("SceneBuilder: Popover rebuild uses updated resolved placement after same-pass measurement") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  scope.store.beginRebuild();

  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 200.f;

  Element popover = Popover{
      .content = Element{Rectangle{}}.size(40.f, 20.f),
      .placement = PopoverPlacement::Below,
      .arrow = true,
      .backgroundColor = Color::hex(0xFFFFFF),
      .borderColor = Color::hex(0xE0E0E6),
      .borderWidth = 1.f,
      .cornerRadius = 10.f,
      .contentPadding = 12.f,
      .resolvedPlacement = PopoverPlacement::Below,
  };

  MeasureContext measureContext{textSystem};
  ComponentKey const rootKey{LocalId::fromIndex(0)};
  measureContext.pushConstraints(constraints, LayoutHints{});
  measureContext.resetTraversalState(rootKey);
  measureContext.setMeasurementRootKey(rootKey);
  measureContext.setCurrentElement(&popover);
  popover.measure(measureContext, constraints, LayoutHints{}, textSystem);

  auto& popoverState = const_cast<Popover&>(popover.as<Popover>());
  popoverState.resolvedPlacement = PopoverPlacement::Above;
  scope.store.discardCurrentRebuildBody(rootKey);

  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  std::unique_ptr<SceneNode> tree = builder.build(popover, NodeId{1ull}, constraints, nullptr, rootKey);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);

  auto* content = dynamic_cast<RectSceneNode*>(tree->children()[1].get());
  REQUIRE(content != nullptr);
  CHECK(content->position.y == doctest::Approx(12.f));
  CHECK(tree->bounds.height == doctest::Approx(20.f + 24.f + PopoverCalloutShape::kArrowH));

  scope.store.endRebuild();
}

namespace {

struct CompositeWrappingIcon {
  auto body() const {
    return Icon{
        .name = IconName::Info,
        .size = 18.f,
    };
  }
};

} // namespace

TEST_CASE("SceneBuilder: composite returning Icon resolves nested composite bodies") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};
  scope.store.beginRebuild();

  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  LayoutConstraints constraints{};
  constraints.maxWidth = 200.f;
  constraints.maxHeight = 120.f;

  std::unique_ptr<SceneNode> tree = builder.build(Element{CompositeWrappingIcon{}}, NodeId{1ull}, constraints);
  REQUIRE(tree != nullptr);

  NullRenderer renderer{};
  render(*tree, renderer);
  CHECK(renderer.textCount == 1);

  scope.store.endRebuild();
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
  flux::ResolvedRootScene const resolved = holder.resolveScene(constraints);
  Element const* sceneRoot = resolved.element;
  REQUIRE(sceneRoot != nullptr);
  CHECK(sceneRoot->typeTag() == ElementType::Text);
  CHECK(resolved.rootKey == ComponentKey{LocalId::fromIndex(0), LocalId::fromIndex(0)});

  SceneBuilder builder{textSystem, EnvironmentStack::current()};
  std::unique_ptr<SceneNode> tree =
      builder.build(*sceneRoot, NodeId{1ull}, constraints, nullptr, resolved.rootKey);
  REQUIRE(tree != nullptr);

  NullRenderer renderer{};
  render(*tree, renderer);
  CHECK(renderer.textCount == 1);

  scope.store.endRebuild();
}
