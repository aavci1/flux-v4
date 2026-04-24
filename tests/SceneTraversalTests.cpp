#include "SceneBuilderTestSupport.hpp"

#include <Flux/UI/HoverController.hpp>

TEST_CASE("Scenegraph interaction lookup preserves focus order and keyed handler lookup") {
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

  scenegraph::SceneGraph graph{builder.build(root, constraints)};

  std::vector<ComponentKey> const order = scenegraph::collectFocusableKeys(graph);
  REQUIRE(order.size() == 2);
  CHECK(order[0] == ComponentKey{LocalId::fromString("a")});
  CHECK(order[1] == ComponentKey{LocalId::fromString("b")});

  auto const [nodeA, interactionA] =
      scenegraph::findInteractionByKey(graph, ComponentKey{LocalId::fromString("a")});
  REQUIRE(nodeA != nullptr);
  REQUIRE(interactionA != nullptr);
  REQUIRE(interactionA->focusable);
  REQUIRE(static_cast<bool>(interactionA->onKeyDown));
  interactionA->onKeyDown(KeyCode{}, Modifiers{});
  CHECK(keyDownA);

  auto const [closestNode, closestInteraction] = scenegraph::findClosestInteractionByKey(
      graph, ComponentKey{LocalId::fromString("b"), LocalId::fromString("child")});
  REQUIRE(closestNode != nullptr);
  REQUIRE(closestInteraction != nullptr);
  REQUIRE(static_cast<bool>(closestInteraction->onKeyDown));
  closestInteraction->onKeyDown(KeyCode{}, Modifiers{});
  CHECK(keyDownB);
}

TEST_CASE("Scenegraph interaction hit testing respects custom transform local coordinates") {
  auto root = std::make_unique<scenegraph::GroupNode>(Rect{0.f, 0.f, 80.f, 60.f});
  auto rect = std::make_unique<scenegraph::RectNode>(Rect{4.f, 3.f, 20.f, 10.f}, FillStyle::solid(Colors::red));
  auto interaction = std::make_unique<scenegraph::InteractionData>();
  interaction->stableTargetKey = ComponentKey{LocalId::fromString("scaled")};
  rect->setInteraction(std::move(interaction));
  rect->setTransform(Mat3::scale(2.f));
  scenegraph::SceneNode* rectPtr = rect.get();
  root->appendChild(std::move(rect));

  scenegraph::SceneGraph graph{std::move(root)};

  auto const hit = scenegraph::hitTestInteraction(graph, Point{18.f, 16.f});
  REQUIRE(hit.has_value());
  CHECK(hit->node == rectPtr);
  CHECK(hit->localPoint.x == doctest::Approx(7.f));
  CHECK(hit->localPoint.y == doctest::Approx(6.5f));
  REQUIRE(hit->interaction != nullptr);
  CHECK(hit->interaction->stableTargetKey == ComponentKey{LocalId::fromString("scaled")});

  auto const local = scenegraph::localPointForNode(graph.root(), Point{18.f, 16.f}, rectPtr);
  REQUIRE(local.has_value());
  CHECK(local->x == doctest::Approx(7.f));
  CHECK(local->y == doctest::Approx(6.5f));
}

TEST_CASE("GestureTracker overlay-scoped taps resolve through the overlay scenegraph") {
  GestureTracker tracker{};

  bool mainTapped = false;
  InteractiveRectScene mainScene = makeInteractiveRectScene("shared", false, [&] { mainTapped = true; });
  scenegraph::SceneGraph mainGraph;
  mainGraph.setRoot(std::move(mainScene.root));

  bool overlayTapped = false;
  ComponentKey observedTapKey{};
  std::optional<OverlayId> observedOverlayScope{};
  InteractiveRectScene overlayScene = makeInteractiveRectScene("shared", false, [&] {
    overlayTapped = true;
    observedTapKey = tracker.pendingTapLeafKey();
    observedOverlayScope = tracker.pendingTapOverlayScope();
  });

  OverlayEntry overlay{};
  overlay.id = OverlayId{42ull};
  overlay.sceneGraph.setRoot(std::move(overlayScene.root));
  std::vector<OverlayEntry const*> overlays{&overlay};

  GestureTracker::PressState released{};
  released.stableTargetKey = ComponentKey{LocalId::fromString("shared")};
  released.hadOnTapOnDown = true;
  released.overlayScope = overlay.id;

  CHECK(tracker.dispatchTap(released, overlays, mainGraph));
  CHECK(overlayTapped);
  CHECK_FALSE(mainTapped);
  CHECK(observedTapKey == released.stableTargetKey);
  REQUIRE(observedOverlayScope.has_value());
  CHECK(observedOverlayScope->value == overlay.id.value);
  CHECK(tracker.pendingTapLeafKey().empty());
  CHECK_FALSE(tracker.pendingTapOverlayScope().has_value());
}

TEST_CASE("GestureTracker overlay press lookup falls back by stable key inside the overlay scenegraph") {
  GestureTracker tracker{};
  InteractiveRectScene mainScene = makeInteractiveRectScene("shared");
  scenegraph::SceneGraph mainGraph;
  mainGraph.setRoot(std::move(mainScene.root));

  InteractiveRectScene overlayScene = makeInteractiveRectScene("shared");
  OverlayEntry overlay{};
  overlay.id = OverlayId{7ull};
  SceneNode* overlayLeaf = overlayScene.leaf;
  overlay.sceneGraph.setRoot(std::move(overlayScene.root));
  std::vector<OverlayEntry const*> overlays{&overlay};

  GestureTracker::PressState press{};
  press.stableTargetKey = ComponentKey{LocalId::fromString("shared")};
  press.overlayScope = overlay.id;

  auto const [resolvedNode, interaction] = tracker.findPressInteraction(press, overlays, mainGraph);
  REQUIRE(interaction != nullptr);
  CHECK(resolvedNode == overlayLeaf);
  CHECK(interaction->stableTargetKey == press.stableTargetKey);
  CHECK(tracker.sceneGraphForPress(press, overlays, mainGraph) == &overlay.sceneGraph);
}

TEST_CASE("FocusController modal overlay rebuild syncs focus from the overlay scenegraph") {
  FocusController focus{};
  InteractiveRectScene overlayScene = makeInteractiveRectScene("dialog-primary", true);

  OverlayEntry overlay{};
  overlay.id = OverlayId{9ull};
  overlay.config.modal = true;
  overlay.sceneGraph.setRoot(std::move(overlayScene.root));

  focus.onOverlayPushed(overlay);
  focus.syncAfterOverlayRebuild(overlay);

  REQUIRE(focus.focusInOverlay().has_value());
  CHECK(focus.focusInOverlay()->value == overlay.id.value);
  CHECK(focus.focusedKey() == ComponentKey{LocalId::fromString("dialog-primary")});
}

TEST_CASE("HoverController marks previous and next hovered subtrees dirty") {
  HoverController hover{};
  StateStore store;
  ComponentKey const firstKey{LocalId::fromString("first")};
  ComponentKey const secondKey{LocalId::fromString("second")};

  hover.setDirtyMarker([&](ComponentKey const& key, std::optional<OverlayId>) {
    store.markCompositeDirty(key);
    return true;
  });

  hover.set(firstKey, std::nullopt);
  hover.set(secondKey, std::nullopt);

  CHECK(store.hasPendingDirtyComponents());
  store.beginRebuild(false);
  CHECK_FALSE(store.shouldForceFullRebuild());
  CHECK(store.isComponentDirty(firstKey));
  CHECK(store.isComponentDirty(secondKey));
  store.endRebuild();
}

TEST_CASE("FocusController routes dirty focus transitions to overlay-scoped stores") {
  FocusController focus{};
  StateStore mainStore;
  StateStore overlayStore;
  ComponentKey const mainKey{LocalId::fromString("main-focus")};
  ComponentKey const overlayKey{LocalId::fromString("overlay-focus")};
  OverlayId const overlayId{11ull};

  focus.setDirtyMarker([&](ComponentKey const& key, std::optional<OverlayId> overlayScope) {
    if (overlayScope == overlayId) {
      overlayStore.markCompositeDirty(key);
      return true;
    }
    if (!overlayScope.has_value()) {
      mainStore.markCompositeDirty(key);
      return true;
    }
    return false;
  });

  focus.set(mainKey, std::nullopt, FocusInputKind::Keyboard);
  focus.set(overlayKey, overlayId, FocusInputKind::Pointer);

  CHECK(mainStore.hasPendingDirtyComponents());
  CHECK(overlayStore.hasPendingDirtyComponents());

  mainStore.beginRebuild(false);
  CHECK_FALSE(mainStore.shouldForceFullRebuild());
  CHECK(mainStore.isComponentDirty(mainKey));
  mainStore.endRebuild();

  overlayStore.beginRebuild(false);
  CHECK_FALSE(overlayStore.shouldForceFullRebuild());
  CHECK(overlayStore.isComponentDirty(overlayKey));
  overlayStore.endRebuild();
}

TEST_CASE("GestureTracker marks press enter and exit on the affected subtree") {
  GestureTracker tracker{};
  StateStore store;
  ComponentKey const pressKey{LocalId::fromString("pressed")};

  tracker.setDirtyMarker([&](ComponentKey const& key, std::optional<OverlayId>) {
    store.markCompositeDirty(key);
    return true;
  });

  tracker.recordPress(pressKey, Point{4.f, 5.f}, true, std::nullopt);
  tracker.clearPress();

  CHECK(store.hasPendingDirtyComponents());
  store.beginRebuild(false);
  CHECK_FALSE(store.shouldForceFullRebuild());
  CHECK(store.isComponentDirty(pressKey));
  store.endRebuild();
}
