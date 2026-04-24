#pragma once

/// \file Flux/UI/FocusController.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Core/ComponentKey.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/Overlay.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace flux {

class StateStore;

/// Owns keyboard focus state for one window.
/// All state changes go through the public mutators; reads come through the
/// query methods. Never touches the scene graph or input events directly.
class FocusController {
public:
  using DirtyMarker = std::function<bool(ComponentKey const&, std::optional<OverlayId>)>;

  /// True when the focused leaf has `key` as a prefix of its stable key,
  /// and the overlay scope of the calling `StateStore` matches `focusInOverlay_`.
  /// Called by `useFocus()` / `useKeyboardFocus()` during `body()`.
  bool isInSubtree(ComponentKey const& key, StateStore const& store) const noexcept;

  bool hasKeyboardOrigin() const noexcept;

  ComponentKey const& focusedKey() const noexcept;
  std::optional<OverlayId> focusInOverlay() const noexcept;

  void setDirtyMarker(DirtyMarker marker);
  void set(ComponentKey const& key, std::optional<OverlayId> overlayScope, FocusInputKind kind);
  void clear();

  void cycleInTree(scenegraph::SceneGraph const& graph, bool reverse, std::optional<OverlayId> overlayId);

  /// Tab cycling across all non-modal overlays and the main tree.
  /// `overlayEntries` are the live overlay entries in stack order (bottom first).
  void cycleNonModal(std::vector<OverlayEntry const*> const& overlayEntries,
                     scenegraph::SceneGraph const& mainGraph,
                     bool reverse);

  /// Focus the first focusable leaf whose key has `subtreeKey` as a prefix.
  void requestInSubtree(ComponentKey const& subtreeKey, scenegraph::SceneGraph const& graph,
                        std::optional<OverlayId> overlayId = std::nullopt);

  /// When the hit target is not focusable (e.g. Text inside Button), focus the first focusable
  /// leaf in \p tree whose key shares a prefix with \p pressedKey (same composite subtree).
  void claimFocusForSubtree(ComponentKey const& pressedKey, scenegraph::SceneGraph const& graph,
                            std::optional<OverlayId> overlayScope);

  /// Called by `BuildOrchestrator` when a modal overlay is pushed.
  void onOverlayPushed(OverlayEntry& entry);

  /// Called when an overlay is removed. `mainGraph` is null during Runtime teardown
  /// (StateStore shutdown) so modal focus restore is skipped.
  void onOverlayRemoved(OverlayEntry const& entry, scenegraph::SceneGraph const* mainGraph);

  /// Called after an overlay rebuild completes.
  void syncAfterOverlayRebuild(OverlayEntry& entry);

  void validateAfterRebuild(scenegraph::SceneGraph const& mainGraph);

private:
  bool markDirty(ComponentKey const& key, std::optional<OverlayId> overlayScope) const;
  void markStateTransition(ComponentKey const& previousKey, std::optional<OverlayId> previousOverlayScope,
                           ComponentKey const& nextKey, std::optional<OverlayId> nextOverlayScope) const;

  DirtyMarker dirtyMarker_{};
  ComponentKey focusedKey_{};
  std::optional<OverlayId> focusInOverlay_{};
  FocusInputKind lastInputKind_{ FocusInputKind::Keyboard };
};

} // namespace flux
