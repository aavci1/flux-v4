#include "SceneBuilderTestSupport.hpp"

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
