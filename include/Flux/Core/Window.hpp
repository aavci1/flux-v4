#pragma once

/// \file Flux/Core/Window.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Action.hpp>
#include <Flux/UI/Environment.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>

namespace flux {

struct RootHolder;
class Element;
struct OverlayConfig;
struct OverlayId;

class Application;
class Canvas;
class PlatformWindow;
namespace scenegraph {
class SceneGraph;
}

class OverlayManager;

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

  /// True after the retained scene tree has been created (first `sceneTree()` call).
  bool hasSceneGraph() const;

  /// Lazily creates the retained scene graph on first access. Does not create the canvas.
  scenegraph::SceneGraph& sceneGraph();
  scenegraph::SceneGraph const& sceneGraph() const;

  /// Request a frame; `Application::exec()` renders all windows when the event pump runs.
  void requestRedraw();

  /// Sets the platform mouse cursor shape. Called by Runtime; safe to call
  /// from any code that has a Window reference.
  void setCursor(Cursor kind);

  /// Like `requestRedraw()` but addressed to a specific window handle.
  static void postRedraw(unsigned int handle);

  /// Drawing only; `Application` wraps each call with `beginFrame` and `present` when handling redraw.
  /// Default implementation clears with `clearColor()` then draws the retained scene tree (if any).
  virtual void render(Canvas& canvas);

  /// Color passed to the retained scene-tree render for the initial canvas clear. Default is transparent;
  /// use an opaque color if the scene has no full-window background rect.
  void setClearColor(Color color);
  Color clearColor() const;
  void setTheme(Theme theme);
  Theme const& theme() const;
  bool wantsTextInput() const;

  /// Pushes content onto the overlay stack. Safe from event handlers and outside build passes.
  /// Returns a handle for `removeOverlay`.
  OverlayId pushOverlay(Element content, OverlayConfig config);

  /// Removes the overlay with the given id; no-op if invalid or already removed. Calls `onDismiss`.
  void removeOverlay(OverlayId id);

  /// Removes all overlays; calls `onDismiss` for each.
  void clearOverlays();

  OverlayManager& overlayManager();
  OverlayManager const& overlayManager() const;

  /// Registers an action descriptor. Must be called before the first build or during window setup —
  /// descriptors are static for the window lifetime. Calling again for the same name replaces it.
  void registerAction(std::string name, ActionDescriptor descriptor);

  /// True if \p name is registered and descriptor + handler enabled checks pass (for menus/toolbars).
  ///
  /// During an active `body()` pass, handler state is read from the **committed** action registry (the
  /// previous rebuild). The in-flight build buffer is not swapped until rebuild completes, so enabled
  /// UI can lag by one frame (e.g. clipboard or selection); the next reactive pass corrects it.
  bool isActionEnabled(std::string const& name) const;

  /// Sets the root view component (declarative UI). Creates internal state on first call.
  /// Definition in `<Flux/Core/WindowUI.hpp>` (include that header in TUs that call `setView`).
  ///
  /// Pass a component with `setView(std::move(c))` when `C` is movable/copyable.
  /// For a default-constructible root whose subcomponents own non-movable state (e.g. `Signal`),
  /// use `setView<C>()` so the root is built in place on the heap (no move of inner state).
  template<typename C>
  void setView(C&& component);

  template<typename C>
  void setView();

  EnvironmentLayer const& environmentLayer() const;

  template<typename T>
  void setEnvironmentValue(T value);

  template<typename T>
  T const* environmentValue() const;

protected:
  friend class Application;

  explicit Window(const WindowConfig& config);

private:
  friend class Runtime;
  friend class InputDispatcher;

  EnvironmentLayer& environmentLayerMut();

  std::unordered_map<std::string, ActionDescriptor> const& actionDescriptors() const;

  /// Used by `Application` (friend); implementation on `Impl`.
  PlatformWindow* platformWindow() const;
  /// Used by `Window::setView` in `<Flux/Core/WindowUI.hpp>`; implementation on `Impl`.
  void setViewRoot(std::unique_ptr<RootHolder> holder);

  struct Impl;
  std::unique_ptr<Impl> d;
};

template<typename T>
void Window::setEnvironmentValue(T value) {
  environmentLayerMut().set(std::move(value));
}

template<typename T>
T const* Window::environmentValue() const {
  return environmentLayer().get<T>();
}

} // namespace flux
