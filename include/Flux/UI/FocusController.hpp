#pragma once

/// \file Flux/UI/FocusController.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Overlay.hpp>

#include <optional>
#include <vector>

namespace flux {

class StateStore;

/// Owns keyboard focus state for one window.
/// All state changes go through the public mutators; reads come through the
/// query methods. Never touches the scene graph or input events directly.
class FocusController {
public:
  /// True when the focused leaf has `key` as a prefix of its stable key,
  /// and the overlay scope of the calling `StateStore` matches `focusInOverlay_`.
  /// Called by `useFocus()` / `useKeyboardFocus()` during `body()`.
  bool isInSubtree(ComponentKey const& key, StateStore const& store) const noexcept;

  bool hasKeyboardOrigin() const noexcept;

  ComponentKey const& focusedKey() const noexcept;
  std::optional<OverlayId> focusInOverlay() const noexcept;

  void set(ComponentKey const& key, std::optional<OverlayId> overlayScope, FocusInputKind kind);
  void clear();

  /// Tab cycling within one EventMap (used for modal overlays).
  void cycleInMap(EventMap const& em, bool reverse, std::optional<OverlayId> overlayId);

  /// Tab cycling across all non-modal overlays and the main tree.
  /// `overlayEntries` are the live overlay entries in stack order (bottom first).
  void cycleNonModal(std::vector<OverlayEntry const*> const& overlayEntries, EventMap const& mainEventMap,
                     bool reverse);

  /// Focus the first focusable leaf whose key has `subtreeKey` as a prefix.
  void requestInSubtree(ComponentKey const& subtreeKey, EventMap const& eventMap,
                        std::optional<OverlayId> overlayId = std::nullopt);

  /// When the hit target is not focusable (e.g. Text inside Button), focus the first focusable
  /// leaf in \p em whose key shares a prefix with \p pressedKey (same composite subtree).
  void claimFocusForSubtree(ComponentKey const& pressedKey, EventMap const& em,
                            std::optional<OverlayId> overlayScope);

  /// Called by `BuildOrchestrator` when a modal overlay is pushed.
  void onOverlayPushed(OverlayEntry& entry);

  /// Called when an overlay is removed. `mainEventMap` is null during Runtime teardown
  /// (StateStore shutdown) so modal focus restore is skipped.
  void onOverlayRemoved(OverlayEntry const& entry, EventMap const* mainEventMap);

  /// Called after an overlay rebuild completes.
  void syncAfterOverlayRebuild(OverlayEntry& entry);

  /// Validate `focusedKey_` against `mainEventMap` after a main-tree rebuild.
  void validateAfterRebuild(EventMap const& mainEventMap);

private:
  ComponentKey focusedKey_{};
  std::optional<OverlayId> focusInOverlay_{};
  FocusInputKind lastInputKind_{ FocusInputKind::Keyboard };
};

} // namespace flux
