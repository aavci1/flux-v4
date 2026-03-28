#pragma once

#include <memory>
#include <string>

#include <Flux/Core/Types.hpp>

namespace flux {

class Application;
class Canvas;
class Element;
class PlatformWindow;
class SceneGraph;

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

  /// True after the scene graph has been created (first `sceneGraph()` call).
  bool hasSceneGraph() const;

  /// Lazily creates the scene graph on first access. Does not create the canvas.
  SceneGraph& sceneGraph();
  SceneGraph const& sceneGraph() const;

  /// Request a frame; `Application::exec()` renders all windows when the event pump runs.
  void requestRedraw();

  /// Like `requestRedraw()`; `handle` is reserved for future per-window scheduling.
  static void postRedraw(unsigned int handle);

  /// Drawing only; `Application` wraps each call with `beginFrame` and `present` when handling redraw.
  /// Default implementation clears with `clearColor()` then draws the scene graph (if any).
  virtual void render(Canvas& canvas);

  /// Color passed to `SceneRenderer::render` for the initial canvas clear. Default is transparent;
  /// use an opaque color if the scene has no full-window background rect.
  void setClearColor(Color color);
  Color clearColor() const;

  /// Sets the root view component (declarative UI). Creates internal state on first call.
  /// Definition in `<Flux/Core/WindowUI.hpp>` (include that header in TUs that call `setView`).
  template<typename C>
  void setView(C component);

protected:
  friend class Application;

  explicit Window(const WindowConfig& config);

private:
  /// Used by `Application` (friend); implementation on `Impl`.
  PlatformWindow* platformWindow() const;
  /// Used by `Window::setView` in `<Flux/Core/WindowUI.hpp>`; implementation on `Impl`.
  void setViewRoot(Element&& root);

  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
