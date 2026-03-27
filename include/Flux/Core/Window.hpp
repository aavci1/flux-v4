#pragma once

#include <memory>
#include <string>

#include <Flux/Core/Types.hpp>

namespace flux {

class Application;
class Canvas;
class PlatformWindow;

struct WindowConfig {
  Size size = {1280, 720};
  std::string title = "Flux Application";
  bool fullscreen = false;
  bool resizable = true;
};

class Window {
public:
  virtual ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  Size getSize() const;
  void setTitle(std::string title);
  void setFullscreen(bool fullscreen);
  unsigned int handle() const;

  /// Lazily creates the backing canvas on first use.
  Canvas& canvas();

  /// Request a frame; `Application::exec()` renders all windows when the event pump runs.
  void requestRedraw();

  /// Like `requestRedraw()`; `handle` is reserved for future per-window scheduling.
  static void postRedraw(unsigned int handle);

  /// Drawing only; `Application` wraps each call with `beginFrame` and `present` when handling redraw.
  virtual void render(Canvas& canvas);

protected:
  friend class Application;

  explicit Window(const WindowConfig& config);

private:
  PlatformWindow* platformWindow() const;

  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
