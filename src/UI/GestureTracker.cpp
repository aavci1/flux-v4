#include <Flux/UI/GestureTracker.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Scene/NodeId.hpp>
#include <Flux/Scene/SceneTreeInteraction.hpp>
#include <Flux/UI/StateStore.hpp>

namespace flux {

namespace {

Point pointInPressTreeRoot(Point windowPoint, OverlayEntry const* overlay) {
  if (!overlay) {
    return windowPoint;
  }
  return Point{
      windowPoint.x - overlay->resolvedFrame.x,
      windowPoint.y - overlay->resolvedFrame.y,
  };
}

} // namespace

void GestureTracker::recordPress(NodeId nodeId, ComponentKey stableTargetKey, Point downPoint, bool hadOnTap,
                                 std::optional<OverlayId> overlayScope) {
  PressState ps{};
  ps.nodeId = nodeId;
  ps.stableTargetKey = std::move(stableTargetKey);
  ps.downPoint = downPoint;
  ps.cancelled = false;
  ps.hadOnTapOnDown = hadOnTap;
  ps.overlayScope = overlayScope;
  activePress_ = std::move(ps);
}

OverlayEntry const* GestureTracker::overlayForPress(PressState const& ps,
                                                    std::vector<OverlayEntry const*> const& overlayEntries) const {
  if (ps.overlayScope.has_value()) {
    for (OverlayEntry const* p : overlayEntries) {
      if (p->id.value == ps.overlayScope->value) {
        return p;
      }
    }
  }
  return nullptr;
}

std::pair<NodeId, InteractionData const*> GestureTracker::findPressInteraction(
    PressState const& ps, std::vector<OverlayEntry const*> const& overlayEntries, SceneTree const& mainTree) const {
  if (OverlayEntry const* overlay = overlayForPress(ps, overlayEntries)) {
    if (auto const [id, interaction] = findInteractionById(overlay->sceneTree, ps.nodeId); interaction) {
      return {id, interaction};
    }
    if (!ps.stableTargetKey.empty()) {
      return findClosestInteractionByKey(overlay->sceneTree, ps.stableTargetKey);
    }
    return {kInvalidNodeId, nullptr};
  }
  if (ps.overlayScope.has_value()) {
    return {kInvalidNodeId, nullptr};
  }
  if (auto const [id, interaction] = findInteractionById(mainTree, ps.nodeId); interaction) {
    return {id, interaction};
  }
  if (!ps.stableTargetKey.empty()) {
    return findClosestInteractionByKey(mainTree, ps.stableTargetKey);
  }
  return {kInvalidNodeId, nullptr};
}

SceneTree const* GestureTracker::sceneTreeForPress(PressState const& ps,
                                                   std::vector<OverlayEntry const*> const& overlayEntries,
                                                   SceneTree const& mainTree) const {
  if (OverlayEntry const* overlay = overlayForPress(ps, overlayEntries)) {
    return &overlay->sceneTree;
  }
  if (ps.overlayScope.has_value()) {
    return nullptr;
  }
  return &mainTree;
}

bool GestureTracker::pressMatchesStoreContext(StateStore const& store) const noexcept {
  if (!activePress_) {
    return false;
  }
  std::optional<std::uint64_t> const oscope = store.overlayScope();
  if (activePress_->overlayScope.has_value()) {
    return oscope.has_value() && *oscope == activePress_->overlayScope->value;
  }
  return !oscope.has_value();
}

void GestureTracker::cancelPress(Point windowPoint, std::vector<OverlayEntry const*> const& overlayEntries,
                                 SceneTree const& mainTree) {
  if (!activePress_) {
    return;
  }
  OverlayEntry const* overlay = overlayForPress(*activePress_, overlayEntries);
  if (SceneTree const* tree = sceneTreeForPress(*activePress_, overlayEntries, mainTree)) {
    auto const [currentId, interaction] = findPressInteraction(*activePress_, overlayEntries, mainTree);
    if (interaction && interaction->onPointerUp && currentId.isValid()) {
      HitTester tester{};
      std::optional<Point> local =
          tester.localPointForNode(*tree, pointInPressTreeRoot(activePress_->downPoint, overlay), currentId);
      if (!local) {
        local = tester.localPointForNode(*tree, pointInPressTreeRoot(windowPoint, overlay), currentId);
      }
      interaction->onPointerUp(local.value_or(Point{0.f, 0.f}));
    }
  }
  activePress_ = std::nullopt;
  Application::instance().markReactiveDirty();
}

bool GestureTracker::dispatchTap(PressState const& released,
                                 std::vector<OverlayEntry const*> const& overlayEntries,
                                 SceneTree const& mainTree) {
  if (released.cancelled || !released.hadOnTapOnDown) {
    return false;
  }
  if (released.stableTargetKey.empty()) {
    return false;
  }

  if (released.overlayScope.has_value()) {
    if (OverlayEntry const* overlay = overlayForPress(released, overlayEntries)) {
      auto const [id, interaction] = findClosestInteractionByKey(overlay->sceneTree, released.stableTargetKey);
      (void)id;
      if (interaction && interaction->onTap) {
        pendingTapLeafKey_ = released.stableTargetKey;
        pendingTapOverlayScope_ = released.overlayScope;
        interaction->onTap();
        pendingTapLeafKey_.clear();
        pendingTapOverlayScope_.reset();
        return true;
      }
    }
    return false;
  }

  auto const [id, interaction] = findClosestInteractionByKey(mainTree, released.stableTargetKey);
  (void)id;
  if (interaction && interaction->onTap) {
    pendingTapLeafKey_ = released.stableTargetKey;
    pendingTapOverlayScope_.reset();
    interaction->onTap();
    pendingTapLeafKey_.clear();
    pendingTapOverlayScope_.reset();
    return true;
  }
  return false;
}

void GestureTracker::clearPress() {
  activePress_ = std::nullopt;
}

void GestureTracker::markCancelled() {
  if (activePress_) {
    activePress_->cancelled = true;
  }
}

bool GestureTracker::hasActivePress() const noexcept {
  return activePress_.has_value();
}

GestureTracker::PressState const* GestureTracker::press() const noexcept {
  return activePress_ ? &*activePress_ : nullptr;
}

ComponentKey const& GestureTracker::activePressKey() const noexcept {
  static ComponentKey const kEmpty{};
  return activePress_ ? activePress_->stableTargetKey : kEmpty;
}

ComponentKey const& GestureTracker::pendingTapLeafKey() const noexcept {
  return pendingTapLeafKey_;
}

std::optional<OverlayId> GestureTracker::pendingTapOverlayScope() const noexcept {
  return pendingTapOverlayScope_;
}

} // namespace flux
