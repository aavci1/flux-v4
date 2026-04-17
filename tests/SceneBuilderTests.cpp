#include <doctest/doctest.h>

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Scene/CustomTransformSceneNode.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/Renderer.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/Scene/SceneTreeInteraction.hpp>
#include <Flux/Scene/TextSceneNode.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/FocusController.hpp>
#include <Flux/UI/GestureTracker.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/MeasureCache.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/SceneGeometryIndex.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <memory>
#include <string_view>
#include <typeindex>

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
  int rectCount = 0;
  int textCount = 0;

  void save() override {}
  void restore() override {}
  void translate(Point) override {}
  void transform(Mat3 const&) override {}
  void clipRect(Rect, bool = false) override {}
  bool quickReject(Rect) const override { return false; }
  void setOpacity(float) override {}
  void setBlendMode(BlendMode) override {}
  void drawRect(Rect const&, CornerRadius const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override {
    ++rectCount;
  }
  void drawLine(Point, Point, StrokeStyle const&) override {}
  void drawPath(Path const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override {}
  void drawTextLayout(TextLayout const&, Point) override { ++textCount; }
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

} // namespace

namespace flux {

struct LayoutContextTestAccess {
  static LayoutContext* create(TextSystem& ts, LayoutEngine& le, LayoutTree& tree, MeasureCache* mc = nullptr,
                               LayoutContext::SubtreeRootMap* retainedRoots = nullptr,
                               std::uint64_t subtreeRootEpoch = 0) {
    return new LayoutContext(ts, le, tree, mc, retainedRoots, subtreeRootEpoch);
  }

  static void destroy(LayoutContext* ctx) { delete ctx; }
};

} // namespace flux

namespace {

struct LayoutContextDeleter {
  void operator()(flux::LayoutContext* p) const { flux::LayoutContextTestAccess::destroy(p); }
};

using LayoutContextPtr = std::unique_ptr<flux::LayoutContext, LayoutContextDeleter>;

Element keyedRect(std::string key, float width, float height) {
  return Element{Rectangle{}}.key(std::move(key)).size(width, height);
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

TEST_CASE("SceneBuilder: composite root exposes a retained scene body with the runtime root key") {
  NullTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  StoreScope scope{};

  flux::TypedRootHolder<HelloRoot> holder{std::in_place};
  flux::LayoutEngine layoutEngine{};
  flux::LayoutTree layoutTree{};
  flux::MeasureCache measureCache{};
  LayoutContextPtr ctx{LayoutContextTestAccess::create(textSystem, layoutEngine, layoutTree, &measureCache)};

  flux::LayoutConstraints constraints{};
  constraints.minWidth = 320.f;
  constraints.minHeight = 320.f;
  constraints.maxWidth = 320.f;
  constraints.maxHeight = 320.f;

  scope.store.beginRebuild(true);
  measureCache.beginBuild(scope.store.shouldForceFullRebuild());
  layoutEngine.resetForBuild();
  ctx->pushConstraints(constraints);
  layoutEngine.setChildFrame(Rect{0.f, 0.f, 320.f, 320.f});
  holder.layoutInto(*ctx);
  ctx->popConstraints();

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
