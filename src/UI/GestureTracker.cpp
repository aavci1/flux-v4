#include <Flux/UI/GestureTracker.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
#include <Flux/SceneGraph/SceneTraversal.hpp>
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

void scheduleReactiveDirtyFallback() {
  if (Application::hasInstance()) {
    Application::instance().markReactiveDirty();
  }
}

} // namespace

void GestureTracker::setDirtyMarker(DirtyMarker marker) {
  dirtyMarker_ = marker;
}

bool GestureTracker::markDirty(ComponentKey const& key, std::optional<OverlayId> overlayScope) const {
  if (key.empty() || !dirtyMarker_) {
    return false;
  }
  return dirtyMarker_(key, overlayScope);
}

void GestureTracker::markStateTransition(std::optional<PressState> const& previous,
                                         std::optional<PressState> const& next) const {
  bool dirty = false;
  if (previous) {
    dirty |= markDirty(previous->stableTargetKey, previous->overlayScope);
  }
  if (next) {
    dirty |= markDirty(next->stableTargetKey, next->overlayScope);
  }
  if (!dirty) {
    scheduleReactiveDirtyFallback();
  }
}

void GestureTracker::recordPress(ComponentKey stableTargetKey, Point downPoint, bool hadOnTap,
                                 std::optional<OverlayId> overlayScope) {
  std::optional<PressState> const previous = activePress_;
  PressState ps{};
  ps.stableTargetKey = std::move(stableTargetKey);
  ps.downPoint = downPoint;
  ps.cancelled = false;
  ps.hadOnTapOnDown = hadOnTap;
  ps.overlayScope = overlayScope;
  activePress_ = std::move(ps);
  markStateTransition(previous, activePress_);
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

std::pair<scenegraph::SceneNode const*, scenegraph::InteractionData const*>
GestureTracker::findPressInteraction(PressState const& ps,
                                     std::vector<OverlayEntry const*> const& overlayEntries,
                                     scenegraph::SceneGraph const& mainGraph) const {
  if (OverlayEntry const* overlay = overlayForPress(ps, overlayEntries)) {
    if (!ps.stableTargetKey.empty()) {
      return scenegraph::findClosestInteractionByKey(overlay->sceneGraph, ps.stableTargetKey);
    }
    return {nullptr, nullptr};
  }
  if (ps.overlayScope.has_value()) {
    return {nullptr, nullptr};
  }
  if (!ps.stableTargetKey.empty()) {
    return scenegraph::findClosestInteractionByKey(mainGraph, ps.stableTargetKey);
  }
  return {nullptr, nullptr};
}

scenegraph::SceneGraph const*
GestureTracker::sceneGraphForPress(PressState const& ps,
                                   std::vector<OverlayEntry const*> const& overlayEntries,
                                   scenegraph::SceneGraph const& mainGraph) const {
  if (OverlayEntry const* overlay = overlayForPress(ps, overlayEntries)) {
    return &overlay->sceneGraph;
  }
  if (ps.overlayScope.has_value()) {
    return nullptr;
  }
  return &mainGraph;
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
                                 scenegraph::SceneGraph const& mainGraph) {
  if (!activePress_) {
    return;
  }
  std::optional<PressState> const previous = activePress_;
  OverlayEntry const* overlay = overlayForPress(*activePress_, overlayEntries);
  if (scenegraph::SceneGraph const* graph = sceneGraphForPress(*activePress_, overlayEntries, mainGraph)) {
    auto const [currentNode, interaction] = findPressInteraction(*activePress_, overlayEntries, mainGraph);
    if (interaction && interaction->onPointerUp && currentNode) {
      std::optional<Point> local = scenegraph::localPointForNode(
          graph->root(), pointInPressTreeRoot(activePress_->downPoint, overlay), currentNode);
      if (!local) {
        local = scenegraph::localPointForNode(
            graph->root(), pointInPressTreeRoot(windowPoint, overlay), currentNode);
      }
      interaction->onPointerUp(local.value_or(Point{0.f, 0.f}));
    }
  }
  activePress_ = std::nullopt;
  markStateTransition(previous, activePress_);
}

bool GestureTracker::dispatchTap(PressState const& released,
                                 std::vector<OverlayEntry const*> const& overlayEntries,
                                 scenegraph::SceneGraph const& mainGraph) {
  if (released.cancelled || !released.hadOnTapOnDown) {
    return false;
  }
  if (released.stableTargetKey.empty()) {
    return false;
  }

  if (released.overlayScope.has_value()) {
    if (OverlayEntry const* overlay = overlayForPress(released, overlayEntries)) {
      auto const [node, interaction] =
          scenegraph::findClosestInteractionByKey(overlay->sceneGraph, released.stableTargetKey);
      (void)node;
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

  auto const [node, interaction] =
      scenegraph::findClosestInteractionByKey(mainGraph, released.stableTargetKey);
  (void)node;
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
  if (!activePress_) {
    return;
  }
  std::optional<PressState> const previous = activePress_;
  activePress_ = std::nullopt;
  markStateTransition(previous, activePress_);
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
