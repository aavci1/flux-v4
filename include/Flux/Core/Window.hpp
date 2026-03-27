#pragma once

#include <memory>
#include <string>

#include <Flux/Core/Types.hpp>

namespace flux {

class Application;
class Canvas;

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

  /// Queue a redraw; the main thread dispatches `WindowEvent::Kind::Redraw`, then runs `beginFrame` → `render` → `present`.
  void requestRedraw();

  /// Post a redraw by window handle (safe from any thread after `handle()` is known; no-op if the window is gone).
  static void postRedraw(unsigned int handle);

  /// Drawing only; `Application` wraps each call with `beginFrame` and `present` when handling redraw.
  virtual void render(Canvas& canvas);

protected:
  friend class Application;

  explicit Window(const WindowConfig& config);

private:
  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
