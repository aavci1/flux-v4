#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/ActionRegistry.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

namespace flux {

class Window;
class LayoutEngine;

/// Internal driver for `Window::setView` — not part of the stable public API surface.
class Runtime {
public:
  explicit Runtime(Window& window);
  ~Runtime();

  void setRoot(std::unique_ptr<RootHolder> holder);

  void handleInput(InputEvent const& e);

  Window& window() noexcept { return window_; }

  /// Valid only during a build pass (`rebuild`); otherwise `nullptr`.
  static Runtime* current() noexcept { return sCurrent; }

  /// True when the focused leaf lies under \p key in the component tree (`focusedKey_` has \p key as prefix).
  bool isFocusInSubtree(ComponentKey const& key) const noexcept;

  /// True when the hovered leaf lies under \p key in the component tree (`hoveredKey_` has \p key as prefix).
  bool isHoverInSubtree(ComponentKey const& key) const noexcept;

  /// Stable key of the node under the active primary-button press, or empty when none.
  ComponentKey const& activePressKey() const noexcept {
    static ComponentKey const kEmpty{};
    if (activePress_) {
      return activePress_->stableTargetKey;
    }
    return kEmpty;
  }

  /// Programmatically focuses the first focusable leaf in the subtree rooted at \p subtreeKey.
  /// Safe to call from event handlers (outside the build pass). No-op if no focusable node with
  /// \p subtreeKey as a key prefix exists. \c focusOrder() is from the last committed rebuild;
  /// calls during \c body() may not see leaves registered later in the same build.
  void requestFocusInSubtree(ComponentKey const& subtreeKey);

  void onOverlayPushed(OverlayEntry& entry);
  void onOverlayRemoved(OverlayEntry const& entry);
  void syncModalOverlayFocusAfterRebuild(OverlayEntry& entry);

  std::optional<Rect> layoutRectForCurrentComponent() const;

  /// Last completed layout union for the composite subtree registered at \p key (same map as
  /// `useLayoutRect`). Safe from event handlers when given a key captured during `body()`.
  std::optional<Rect> layoutRectForKey(ComponentKey const& key) const;

  /// During a pointer-originated `onTap`, the layout union for the innermost composite ancestor of
  /// the tapped node (longest prefix of the leaf `stableTargetKey` present in the layout cache).
  /// Empty when the tap did not come from a primary press (e.g. keyboard) or outside `onTap`.
  std::optional<Rect> layoutRectForTapAnchor() const;

  /// Longest-prefix layout rect for a leaf stable key (same lookup as `layoutRectForTapAnchor`).
  /// Used by overlay anchor tracking when the popover should follow a scrolled control.
  std::optional<Rect> layoutRectForLeafKeyPrefix(ComponentKey const& stableTargetKey) const;

  /// While inside `onTap` from a primary pointer release, the leaf `stableTargetKey` used for tap
  /// anchor layout. Empty after `onTap` returns.
  std::optional<ComponentKey> tapAnchorLeafKeySnapshot() const;

  /// Layer-local rect for the layout slot of the component currently being built. Valid only during
  /// `rebuild` (e.g. inside `body()`). Matches `ctx.layoutEngine().childFrame()` when wiring leaves.
  Rect buildSlotRect() const { return layoutEngine_.childFrame(); }

  /// For `usePress`: true when `activePress_` overlay scope matches `store.overlayScope()`.
  bool pressKeyMatchesStoreContext(StateStore const& store) const;

  /// True after `~Runtime`'s destructor body finishes; member subobjects are being destroyed.
  /// `onOverlayRemoved` uses this to skip focus/eventMap work during `~StateStore` teardown.
  bool imploding() const noexcept { return imploding_; }

  /// Whether the committed registry's handler for \p name passes enabled checks (for `Window::isActionEnabled`).
  /// Committed data is from the last finished rebuild — not the build currently in progress.
  bool isActionCurrentlyEnabled(std::string const& name) const;

  /// True when focus last moved via Tab / cycle focus / `requestFocusInSubtree` (not pointer down).
  bool isLastFocusFromKeyboard() const noexcept { return lastFocusInputKind_ == FocusInputKind::Keyboard; }

  /// Registry filled during the current rebuild; swap to committed at end of rebuild.
  ActionRegistry& actionRegistryForBuild() noexcept { return actionRegistryBuild_; }

private:
  /// `sizeOverride` — size from `WindowEvent::size` on resize (preferred over re-querying the view).
  void rebuild(std::optional<Size> sizeOverride = std::nullopt);
  void rebuild(Size const& sizeFromResizeEvent) { rebuild(std::optional<Size>(sizeFromResizeEvent)); }
  void subscribeToRebuild();
  void subscribeInput();
  void subscribeWindowEvents();
  void cancelActivePress(Point windowPoint);
  void updateCursorForPoint(Point windowPoint);
  void updateHoveredForPoint(Point windowPoint);
  void applyCursor(Cursor kind);
  void setHovered(ComponentKey const& key, std::optional<OverlayId> overlayScope);
  void clearHovered();
  void setFocus(ComponentKey const& key, std::optional<OverlayId> overlayScope, FocusInputKind kind);
  void clearFocus();
  void cycleTabFocusNonModal(bool reverse);
  void cycleTabFocusInMap(EventMap const& em, bool reverse, std::optional<OverlayId> overlayId);
  void fillLayoutRectCache(SceneGraph const& graph, BuildContext const& ctx);
  std::optional<Rect> layoutRectForTapLeafKey(ComponentKey const& stableTargetKey) const;

  static thread_local Runtime* sCurrent;

  struct PressState {
    NodeId nodeId{};
    /// Same logical target as `EventHandlers::stableTargetKey`; used when `nodeId` is stale after rebuild.
    ComponentKey stableTargetKey{};
    Point downPoint{};
    bool cancelled = false;
    /// True if the press target had `onTap` at PointerDown (used when the scene rebuilds before PointerUp).
    bool hadOnTapOnDown = false;
    /// When the press started on an overlay; `findPressHandlersWithNode` resolves against that overlay's `EventMap`.
    std::optional<OverlayId> overlayScope{};
  };
  std::pair<NodeId, EventHandlers const*> findPressHandlersWithNode(PressState const& ps) const;
  SceneGraph const& sceneGraphForPress(PressState const& ps) const;
  std::optional<PressState> activePress_{};

  Window& window_;
  std::unique_ptr<RootHolder> rootHolder_;
  EventMap eventMap_;
  LayoutEngine layoutEngine_;
  ObserverHandle rebuildHandle_{};
  bool inputRegistered_ = false;

  /// Stable key of the focused node (leaf `stableTargetKey`). Empty when nothing is focused.
  ComponentKey focusedKey_{};
  /// Updated whenever `setFocus` runs (not when focus is assigned by direct `focusedKey_ =` in overlay restore).
  FocusInputKind lastFocusInputKind_{FocusInputKind::Keyboard};
  /// Stable key of the node under the pointer (geometric hover). Empty when nothing is hovered.
  ComponentKey hoveredKey_{};
  /// When set, `focusedKey_` is resolved against that overlay's `EventMap`.
  std::optional<OverlayId> focusInOverlay_{};
  /// When set, `hoveredKey_` is for that overlay's scene graph.
  std::optional<OverlayId> hoverInOverlay_{};
  /// Set true at end of `~Runtime()` body (before members are destroyed). Outlives `stateStore_`
  /// so `OverlayHookSlot::~` → `onOverlayRemoved` can see teardown and avoid invalid `eventMap_` use.
  bool imploding_{false};
  /// Declared after overlay/focus fields so `~StateStore` (e.g. `OverlayHookSlot` calling
  /// `removeOverlay` → `onOverlayRemoved`) runs while `focusInOverlay_` / `eventMap_` / etc. are still alive.
  StateStore stateStore_;
  /// When false, keyboard events are not dispatched (window in background).
  bool windowHasFocus_ = true;

  Cursor currentCursor_ = Cursor::Arrow;

  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> layoutRectPrev_{};
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> layoutRectCurrent_{};

  /// Set immediately before `EventHandlers::onTap` for a pointer release; cleared after `onTap` returns.
  ComponentKey pendingTapTargetKey_{};

  ActionRegistry actionRegistryBuild_{};
  ActionRegistry actionRegistryCommitted_{};
};

} // namespace flux
