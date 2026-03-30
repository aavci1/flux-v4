#include <Flux/UI/GestureTracker.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/HitTestUtil.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>

namespace flux {

bool GestureTracker::keySharesPrefix(ComponentKey const& a, ComponentKey const& b) noexcept {
  if (a.empty() || b.empty()) {
    return false;
  }
  std::size_t const len = std::min(a.size(), b.size());
  return std::equal(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(len), b.begin());
}

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
    PressState const& ps, std::vector<OverlayEntry const*> const& overlayEntries,
    EventMap const& mainEventMap) const {
  if (ps.overlayScope.has_value()) {
    for (OverlayEntry const* p : overlayEntries) {
      OverlayEntry const& e = *p;
      if (e.id.value != ps.overlayScope->value) {
        continue;
      }
      if (EventHandlers const* h = e.eventMap.find(ps.nodeId)) {
        return { ps.nodeId, h };
      }
      if (!ps.stableTargetKey.empty()) {
        return e.eventMap.findWithIdByKey(ps.stableTargetKey);
      }
      return { kInvalidNodeId, nullptr };
    }
    return { kInvalidNodeId, nullptr };
  }
  if (EventHandlers const* h = mainEventMap.find(ps.nodeId)) {
    return { ps.nodeId, h };
  }
  if (!ps.stableTargetKey.empty()) {
    return mainEventMap.findWithIdByKey(ps.stableTargetKey);
  }
  return { kInvalidNodeId, nullptr };
}

SceneGraph const* GestureTracker::sceneGraphForPress(PressState const& ps,
                                                     std::vector<OverlayEntry const*> const& overlayEntries,
                                                     SceneGraph const& mainGraph) const {
  if (ps.overlayScope.has_value()) {
    for (OverlayEntry const* p : overlayEntries) {
      OverlayEntry const& e = *p;
      if (e.id.value == ps.overlayScope->value) {
        return &e.graph;
      }
    }
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
                                 SceneGraph const& mainGraph, EventMap const& mainEventMap) {
  if (!activePress_) {
    return;
  }
  SceneGraph const& graph = *sceneGraphForPress(*activePress_, overlayEntries, mainGraph);
  auto const [currentId, h] = findPressHandlers(*activePress_, overlayEntries, mainEventMap);
  if (h && h->onPointerUp && currentId.isValid()) {
    HitTester tester{};
    std::optional<Point> local = tester.localPointForNode(graph, activePress_->downPoint, currentId);
    if (!local) {
      local = tester.localPointForNode(graph, windowPoint, currentId);
    }
    h->onPointerUp(local.value_or(Point{ 0.f, 0.f }));
  }
  activePress_ = std::nullopt;
  Application::instance().markReactiveDirty();
}

bool GestureTracker::dispatchTap(PressState const& released, Point upPoint,
                                 std::vector<OverlayEntry const*> const& overlayEntries,
                                 SceneGraph const& mainGraph, EventMap const& mainEventMap) {
  if (released.cancelled || !released.hadOnTapOnDown) {
    return false;
  }
  if (released.stableTargetKey.empty()) {
    return false;
  }

  for (OverlayEntry const* p : overlayEntries) {
    OverlayEntry const& oe = *p;
    Point const pl{ upPoint.x - oe.resolvedFrame.x, upPoint.y - oe.resolvedFrame.y };
    if (auto hit = hitTestPointerTarget(oe.eventMap, oe.graph, pl)) {
      if (EventHandlers const* h = oe.eventMap.find(hit->nodeId)) {
        if (h->onTap && keySharesPrefix(h->stableTargetKey, released.stableTargetKey)) {
          pendingTapLeafKey_ = released.stableTargetKey;
          h->onTap();
          pendingTapLeafKey_.clear();
          return true;
        }
      }
      return false;
    }
  }

  if (auto hit = hitTestPointerTarget(mainEventMap, mainGraph, upPoint)) {
    if (EventHandlers const* h = mainEventMap.find(hit->nodeId)) {
      if (h->onTap && keySharesPrefix(h->stableTargetKey, released.stableTargetKey)) {
        pendingTapLeafKey_ = released.stableTargetKey;
        h->onTap();
        pendingTapLeafKey_.clear();
        return true;
      }
    }
  }
  return false;
}

} // namespace flux
