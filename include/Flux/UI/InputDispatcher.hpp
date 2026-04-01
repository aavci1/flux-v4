#pragma once

/// \file Flux/UI/InputDispatcher.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Events.hpp>

#include <vector>

namespace flux {

struct OverlayEntry;

class FocusController;
class HoverController;
class GestureTracker;
class CursorController;
class BuildOrchestrator;
class Window;

class InputDispatcher {
public:
  InputDispatcher(Window& window, FocusController& focus, HoverController& hover, GestureTracker& gesture,
                    CursorController& cursor, BuildOrchestrator& build, bool& windowHasFocus);

  void dispatch(InputEvent const& e);

private:
  void onKeyDown(InputEvent const& e);
  void onKeyUp(InputEvent const& e);
  void onTextInput(InputEvent const& e);
  void onScroll(InputEvent const& e);
  void onPointerDown(InputEvent const& e);
  void onPointerMove(InputEvent const& e);
  void onPointerUp(InputEvent const& e);

  std::vector<OverlayEntry const*> overlayEntriesTopFirst() const;
  std::vector<OverlayEntry const*> overlayEntriesBottomFirst() const;

  Window& window_;
  FocusController& focus_;
  HoverController& hover_;
  GestureTracker& gesture_;
  CursorController& cursor_;
  BuildOrchestrator& build_;
  bool& windowHasFocus_;
};

} // namespace flux
