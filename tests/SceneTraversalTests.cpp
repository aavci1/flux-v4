#include "SceneBuilderTestSupport.hpp"

#include <Flux/UI/HoverController.hpp>

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

  tracker.recordPress(NodeId{3ull}, pressKey, Point{4.f, 5.f}, true, std::nullopt);
  tracker.clearPress();

  CHECK(store.hasPendingDirtyComponents());
  store.beginRebuild(false);
  CHECK_FALSE(store.shouldForceFullRebuild());
  CHECK(store.isComponentDirty(pressKey));
  store.endRebuild();
}
