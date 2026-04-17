#include <Flux/UI/GestureTracker.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Scene/NodeId.hpp>
#include <Flux/Scene/SceneTreeInteraction.hpp>
#include <Flux/UI/StateStore.hpp>

namespace flux {

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

std::pair<NodeId, EventHandlers const*> GestureTracker::findPressHandlers(
    PressState const& ps, std::vector<OverlayEntry const*> const& overlayEntries) const {
  if (ps.overlayScope.has_value()) {
    for (OverlayEntry const* p : overlayEntries) {
      OverlayEntry const& e = *p;
      if (e.id.value != ps.overlayScope->value) {
        continue;
      }
      if (EventHandlers const* h = e.eventMap.find(ps.nodeId)) {
        return {ps.nodeId, h};
      }
      if (!ps.stableTargetKey.empty()) {
        return e.eventMap.findClosestWithIdByKey(ps.stableTargetKey);
      }
      return {kInvalidNodeId, nullptr};
    }
    return {kInvalidNodeId, nullptr};
  }
  return {kInvalidNodeId, nullptr};
}

std::pair<NodeId, InteractionData const*> GestureTracker::findPressInteraction(PressState const& ps,
                                                                               SceneTree const& mainTree) const {
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

SceneGraph const* GestureTracker::sceneGraphForPress(PressState const& ps,
                                                     std::vector<OverlayEntry const*> const& overlayEntries) const {
  if (ps.overlayScope.has_value()) {
    for (OverlayEntry const* p : overlayEntries) {
      OverlayEntry const& e = *p;
      if (e.id.value == ps.overlayScope->value) {
        return &e.graph;
      }
    }
  }
  return nullptr;
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
  if (activePress_->overlayScope.has_value()) {
    if (SceneGraph const* graph = sceneGraphForPress(*activePress_, overlayEntries)) {
      auto const [currentId, h] = findPressHandlers(*activePress_, overlayEntries);
      if (h && h->onPointerUp && currentId.isValid()) {
        HitTester tester{};
        std::optional<Point> local = tester.localPointForNode(*graph, activePress_->downPoint, currentId);
        if (!local) {
          local = tester.localPointForNode(*graph, windowPoint, currentId);
        }
        h->onPointerUp(local.value_or(Point{0.f, 0.f}));
      }
    }
  } else {
    auto const [currentId, interaction] = findPressInteraction(*activePress_, mainTree);
    if (interaction && interaction->onPointerUp && currentId.isValid()) {
      HitTester tester{};
      std::optional<Point> local = tester.localPointForNode(mainTree, activePress_->downPoint, currentId);
      if (!local) {
        local = tester.localPointForNode(mainTree, windowPoint, currentId);
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
    for (OverlayEntry const* p : overlayEntries) {
      if (p->id != *released.overlayScope) {
        continue;
      }
      auto const [id, h] = p->eventMap.findClosestWithIdByKey(released.stableTargetKey);
      (void)id;
      if (h && h->onTap) {
        pendingTapLeafKey_ = released.stableTargetKey;
        h->onTap();
        pendingTapLeafKey_.clear();
        return true;
      }
      return false;
    }
    return false;
  }

  auto const [id, interaction] = findClosestInteractionByKey(mainTree, released.stableTargetKey);
  (void)id;
  if (interaction && interaction->onTap) {
    pendingTapLeafKey_ = released.stableTargetKey;
    interaction->onTap();
    pendingTapLeafKey_.clear();
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

} // namespace flux
