#pragma once

/// \file Flux/UI/GestureTracker.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Scene/InteractionData.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/UI/Overlay.hpp>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace flux {

class StateStore;

/// Owns the active pointer-press state and tap dispatch for one window.
class GestureTracker {
public:
  using DirtyMarker = std::function<bool(ComponentKey const&, std::optional<OverlayId>)>;

  struct PressState {
    NodeId nodeId{};
    ComponentKey stableTargetKey{};
    Point downPoint{};
    bool cancelled = false;
    bool hadOnTapOnDown = false;
    std::optional<OverlayId> overlayScope{};
  };

  void setDirtyMarker(DirtyMarker marker);

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
  std::optional<OverlayId> pendingTapOverlayScope() const noexcept;

  OverlayEntry const* overlayForPress(PressState const& ps,
                                      std::vector<OverlayEntry const*> const& overlayEntries) const;
  std::pair<NodeId, InteractionData const*> findPressInteraction(
      PressState const& ps, std::vector<OverlayEntry const*> const& overlayEntries, SceneTree const& mainTree) const;
  SceneTree const* sceneTreeForPress(PressState const& ps,
                                     std::vector<OverlayEntry const*> const& overlayEntries,
                                     SceneTree const& mainTree) const;

private:
  bool markDirty(ComponentKey const& key, std::optional<OverlayId> overlayScope) const;
  void markStateTransition(std::optional<PressState> const& previous, std::optional<PressState> const& next) const;

  DirtyMarker dirtyMarker_{};
  std::optional<PressState> activePress_{};
  ComponentKey pendingTapLeafKey_{};
  std::optional<OverlayId> pendingTapOverlayScope_{};
};

} // namespace flux
