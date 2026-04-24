#pragma once

#include <memory>
#include <string>

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>

namespace flux {

class Window;
class Canvas;

/// Internal abstract platform window; implemented in platform translation units. Not part of the public API.
class PlatformWindow {
public:
  virtual ~PlatformWindow() = default;

  virtual void setFluxWindow(Window* window) = 0;

  /// Present the native window after the Flux `Window` is registered and `setFluxWindow` has run.
  /// Implementations should not order the window on screen before this (so lifecycle callbacks see a
  /// valid `Window*`). Default: no-op.
  virtual void show() {}

  virtual std::unique_ptr<Canvas> createCanvas(Window& owner) = 0;

  virtual void resize(const Size& newSize) = 0;
  virtual void setFullscreen(bool fullscreen) = 0;
  virtual void setTitle(const std::string& title) = 0;

  virtual Size currentSize() const = 0;
  virtual bool isFullscreen() const = 0;
  virtual unsigned int handle() const = 0;

  virtual void* nativeGraphicsSurface() const = 0;

  /// Drain queued AppKit/SDL events without blocking (used when a redraw is already pending).
  virtual void processEvents() {}

  /// Block until the next event or `timeoutMs` elapses; `timeoutMs < 0` waits indefinitely.
  virtual void waitForEvents(int /*timeoutMs*/) {}

  /// Wake `waitForEvents` (e.g. after `requestRedraw`).
  virtual void wakeEventLoop() {}

  /// Arm the platform frame pump for the next display boundary.
  virtual void requestAnimationFrame() {}

  /// Marks the most recent frame boundary event as handled by the application loop.
  virtual void acknowledgeAnimationFrameTick() {}

  /// Called after a frame has been presented. `needsAnotherFrame` keeps the frame pump running.
  virtual void completeAnimationFrame(bool /*needsAnotherFrame*/) {}

  virtual void setCursor(Cursor /*kind*/) {}
};

} // namespace flux
