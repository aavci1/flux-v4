#pragma once

/// \file Flux/UI/GestureTracker.hpp
///
/// Part of the Flux public API.


#include <Flux/Scene/InteractionData.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/Scene/SceneGraph.hpp>
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
                   SceneTree const& mainTree);

  void clearPress();

  void markCancelled();

  bool hasActivePress() const noexcept;
  PressState const* press() const noexcept;

  ComponentKey const& activePressKey() const noexcept;

  bool pressMatchesStoreContext(StateStore const& store) const noexcept;

  bool dispatchTap(PressState const& released, std::vector<OverlayEntry const*> const& overlayEntries,
                   SceneTree const& mainTree);

  ComponentKey const& pendingTapLeafKey() const noexcept;

  std::pair<NodeId, EventHandlers const*> findPressHandlers(PressState const& ps,
                                                            std::vector<OverlayEntry const*> const& overlayEntries) const;

  std::pair<NodeId, InteractionData const*> findPressInteraction(PressState const& ps,
                                                                 SceneTree const& mainTree) const;

  SceneGraph const* sceneGraphForPress(PressState const& ps,
                                       std::vector<OverlayEntry const*> const& overlayEntries) const;

private:
  std::optional<PressState> activePress_{};
  ComponentKey pendingTapLeafKey_{};
};

} // namespace flux
