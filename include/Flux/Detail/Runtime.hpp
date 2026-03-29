#pragma once

#include <Flux/Core/Events.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/StateStore.hpp>

#include <memory>
#include <optional>
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

  /// Valid only during a build pass (`rebuild`); otherwise `nullptr`.
  static Runtime* current() noexcept { return sCurrent; }

  /// True when the focused leaf lies under \p key in the component tree (`focusedKey_` has \p key as prefix).
  bool isFocusInSubtree(ComponentKey const& key) const noexcept;

private:
  /// `sizeOverride` — size from `WindowEvent::size` on resize (preferred over re-querying the view).
  void rebuild(std::optional<Size> sizeOverride = std::nullopt);
  void rebuild(Size const& sizeFromResizeEvent) { rebuild(std::optional<Size>(sizeFromResizeEvent)); }
  void subscribeToRebuild();
  void subscribeInput();
  void subscribeWindowEvents();
  void cancelActivePress(Point windowPoint);
  void setFocus(ComponentKey const& key);
  void clearFocus();
  void cycleTabFocus(bool reverse);

  static thread_local Runtime* sCurrent;

  struct PressState {
    NodeId nodeId{};
    /// Same logical target as `EventHandlers::stableTargetKey`; used when `nodeId` is stale after rebuild.
    ComponentKey stableTargetKey{};
    Point downPoint{};
    bool cancelled = false;
    /// True if the press target had `onTap` at PointerDown (used when the scene rebuilds before PointerUp).
    bool hadOnTapOnDown = false;
  };
  std::pair<NodeId, EventHandlers const*> findPressHandlersWithNode(PressState const& ps) const;
  std::optional<PressState> activePress_{};

  Window& window_;
  std::unique_ptr<RootHolder> rootHolder_;
  EventMap eventMap_;
  LayoutEngine layoutEngine_;
  StateStore stateStore_;
  ObserverHandle rebuildHandle_{};
  bool inputRegistered_ = false;

  /// Stable key of the focused node (leaf `stableTargetKey`). Empty when nothing is focused.
  ComponentKey focusedKey_{};
  /// When false, keyboard events are not dispatched (window in background).
  bool windowHasFocus_ = true;
};

} // namespace flux
