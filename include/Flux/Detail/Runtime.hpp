#pragma once

#include <Flux/UI/BuildOrchestrator.hpp>
#include <Flux/UI/CursorController.hpp>
#include <Flux/UI/FocusController.hpp>
#include <Flux/UI/GestureTracker.hpp>
#include <Flux/UI/HoverController.hpp>
#include <Flux/UI/InputDispatcher.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Overlay.hpp>

#include <memory>
#include <optional>
#include <string>

namespace flux {

class Window;

class Runtime {
public:
  explicit Runtime(Window& window);
  ~Runtime();

  void setRoot(std::unique_ptr<RootHolder> holder);
  void handleInput(InputEvent const& e);
  Window& window() noexcept { return window_; }

  static Runtime* current() noexcept { return sCurrent; }

  FocusController& focus() noexcept { return focus_; }
  HoverController& hover() noexcept { return hover_; }
  GestureTracker& gesture() noexcept { return gesture_; }
  LayoutRectCache& layoutRects() noexcept { return buildOrchestrator_.layoutRects(); }

  ActionRegistry& actionRegistryForBuild() noexcept { return buildOrchestrator_.actionRegistryForBuild(); }

  EventMap const& mainEventMap() const noexcept { return buildOrchestrator_.mainEventMap(); }

  bool isActionCurrentlyEnabled(std::string const& name) const;

  void requestFocusInSubtree(ComponentKey const& subtreeKey) {
    focus_.requestInSubtree(subtreeKey, mainEventMap());
  }

  std::optional<Rect> layoutRectForCurrentComponent() const;
  std::optional<Rect> layoutRectForKey(ComponentKey const& key) const;
  std::optional<Rect> layoutRectForTapAnchor() const;
  std::optional<Rect> layoutRectForLeafKeyPrefix(ComponentKey const& stableTargetKey) const;
  std::optional<ComponentKey> tapAnchorLeafKeySnapshot() const;

  Rect buildSlotRect() const { return buildOrchestrator_.buildSlotRect(); }

  bool shuttingDown() const noexcept { return shuttingDown_; }

  /// Alias for overlay teardown checks (same as `shuttingDown()`).
  bool imploding() const noexcept { return shuttingDown_; }

  void onOverlayPushed(OverlayEntry& entry);
  void onOverlayRemoved(OverlayEntry const& entry);
  void syncModalOverlayFocusAfterRebuild(OverlayEntry& entry);

private:
  void rebuild(std::optional<Size> sizeOverride = std::nullopt);
  void subscribeInput();
  void subscribeWindowEvents();

  static thread_local Runtime* sCurrent;

  Window& window_;
  FocusController focus_{};
  HoverController hover_{};
  GestureTracker gesture_{};
  CursorController cursor_;
  BuildOrchestrator buildOrchestrator_;
  InputDispatcher dispatcher_;
  bool windowHasFocus_ = true;
  bool shuttingDown_ = false;
  bool inputRegistered_ = false;
};

} // namespace flux
