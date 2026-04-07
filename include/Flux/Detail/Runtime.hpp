#pragma once

/// \file Flux/Detail/Runtime.hpp
///
/// Part of the Flux public API.


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
  Window& window() noexcept;

  static Runtime* current() noexcept;

  FocusController& focus() noexcept;
  HoverController& hover() noexcept;
  GestureTracker& gesture() noexcept;
  LayoutRectCache& layoutRects() noexcept;

  ActionRegistry& actionRegistryForBuild() noexcept;

  EventMap const& mainEventMap() const noexcept;

  bool isActionCurrentlyEnabled(std::string const& name) const;

  void requestFocusInSubtree(ComponentKey const& subtreeKey);

  std::optional<Rect> layoutRectForCurrentComponent() const;
  std::optional<Rect> layoutRectForKey(ComponentKey const& key) const;
  std::optional<Rect> layoutRectForTapAnchor() const;
  std::optional<Rect> layoutRectForLeafKeyPrefix(ComponentKey const& stableTargetKey) const;
  std::optional<ComponentKey> tapAnchorLeafKeySnapshot() const;

  Rect buildSlotRect() const;

  bool shuttingDown() const noexcept;

  /// Alias for overlay teardown checks (same as `shuttingDown()`).
  bool imploding() const noexcept;

  /// When true, \ref Window::render draws a semi-transparent wireframe overlay for each layout node.
  bool layoutOverlayEnabled() const noexcept { return layoutOverlayEnabled_; }
  void setLayoutOverlayEnabled(bool enabled) noexcept { layoutOverlayEnabled_ = enabled; }

  /// When true, \ref Window::render draws the text-cache stats panel (independent of layout wireframes).
  bool textCacheOverlayEnabled() const noexcept { return textCacheOverlayEnabled_; }
  void setTextCacheOverlayEnabled(bool enabled) noexcept { textCacheOverlayEnabled_ = enabled; }

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
  bool layoutOverlayEnabled_ = false;
  bool textCacheOverlayEnabled_ = false;
};

} // namespace flux
