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

  /// For `usePress`: true when `activePress_` overlay scope matches `store.overlayScope()`.
  bool pressKeyMatchesStoreContext(StateStore const& store) const;

  /// True after `~Runtime`'s destructor body finishes; member subobjects are being destroyed.
  /// `onOverlayRemoved` uses this to skip focus/eventMap work during `~StateStore` teardown.
  bool imploding() const noexcept { return imploding_; }

  /// Descriptor + committed registry: whether the handler for \p name is enabled (for `Window::isActionEnabled`).
  bool isActionCurrentlyEnabled(std::string const& name) const;

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
  void setFocus(ComponentKey const& key, std::optional<OverlayId> overlayScope);
  void clearFocus();
  void cycleTabFocusNonModal(bool reverse);
  void cycleTabFocusInMap(EventMap const& em, bool reverse, std::optional<OverlayId> overlayId);
  void fillLayoutRectCache(SceneGraph const& graph, BuildContext const& ctx);

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

  ActionRegistry actionRegistryBuild_{};
  ActionRegistry actionRegistryCommitted_{};
};

} // namespace flux
