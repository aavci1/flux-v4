#pragma once

#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Core/Types.hpp>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace flux {

class StateStore;

/// Owns the active pointer-press state and tap dispatch for one window.
class GestureTracker {
public:
  struct PressState {
    NodeId nodeId{};
    ComponentKey stableTargetKey{};
    Point downPoint{};
    bool cancelled = false;
    bool hadOnTapOnDown = false;
    std::optional<OverlayId> overlayScope{};
  };

  void recordPress(NodeId nodeId, ComponentKey stableTargetKey, Point downPoint, bool hadOnTap,
                   std::optional<OverlayId> overlayScope);

  void cancelPress(Point cancelPoint, std::vector<OverlayEntry const*> const& overlayEntries,
                   SceneGraph const& mainGraph, EventMap const& mainEventMap);

  void clearPress() { activePress_ = std::nullopt; }

  void markCancelled() {
    if (activePress_) {
      activePress_->cancelled = true;
    }
  }

  bool hasActivePress() const noexcept { return activePress_.has_value(); }
  PressState const* press() const noexcept { return activePress_ ? &*activePress_ : nullptr; }

  ComponentKey const& activePressKey() const noexcept {
    static ComponentKey const kEmpty{};
    return activePress_ ? activePress_->stableTargetKey : kEmpty;
  }

  bool pressMatchesStoreContext(StateStore const& store) const noexcept;

  bool dispatchTap(PressState const& released, Point upPoint,
                   std::vector<OverlayEntry const*> const& overlayEntries, SceneGraph const& mainGraph,
                   EventMap const& mainEventMap);

  ComponentKey const& pendingTapLeafKey() const noexcept { return pendingTapLeafKey_; }

  std::pair<NodeId, EventHandlers const*> findPressHandlers(PressState const& ps,
                                                            std::vector<OverlayEntry const*> const& overlayEntries,
                                                            EventMap const& mainEventMap) const;

  SceneGraph const* sceneGraphForPress(PressState const& ps,
                                       std::vector<OverlayEntry const*> const& overlayEntries,
                                       SceneGraph const& mainGraph) const;

private:
  static bool keySharesPrefix(ComponentKey const& a, ComponentKey const& b) noexcept;

  std::optional<PressState> activePress_{};
  ComponentKey pendingTapLeafKey_{};
};

} // namespace flux
