#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/SceneRenderer.hpp>
#include <Flux/UI/Overlay.hpp>

#include <memory>

#include "Core/PlatformWindow.hpp"
#include "Core/PlatformWindowCreate.hpp"
#include <optional>

namespace flux {

struct Window::Impl {
  std::unique_ptr<PlatformWindow> platform_;
  std::unique_ptr<Canvas> canvas_;
  std::optional<SceneGraph> sceneGraph_;
  Color clearColor_{Colors::lightGray};
  /// Declared before `runtime_` so `~Runtime` (and `OverlayHookSlot` teardown calling `removeOverlay`)
  /// runs while `OverlayManager` is still alive. Reverse member destruction order would destroy
  /// `overlayMgr_` first and use-after-free on window close with an open overlay.
  OverlayManager overlayMgr_;
  std::unique_ptr<Runtime> runtime_;

  explicit Impl(Window&) {}
  ~Impl();

  PlatformWindow* platformWindow() const { return platform_.get(); }
  void setViewRoot(Window& window, std::unique_ptr<RootHolder> holder);
};

void Window::Impl::setViewRoot(Window& window, std::unique_ptr<RootHolder> holder) {
  if (!runtime_) {
    runtime_ = std::make_unique<Runtime>(window);
  }
  runtime_->setRoot(std::move(holder));
}

Window::Impl::~Impl() {
  if (runtime_) {
    overlayMgr_.clear(runtime_.get(), false);
  }
}

Window::Window(const WindowConfig& config) {
  d = std::make_unique<Impl>(*this);
  d->platform_ = detail::createPlatformWindow(config);
  d->platform_->setFluxWindow(this);
  Application::instance().eventQueue().post(WindowLifecycleEvent{
      WindowLifecycleEvent::Kind::Registered,
      handle(),
      this,
  });
}

Window::~Window() {
  const unsigned int id = handle();
  if (Application::hasInstance()) {
    Application::instance().unregisterWindowHandle(id);
    Application::instance().eventQueue().post(
        WindowLifecycleEvent{WindowLifecycleEvent::Kind::Unregistered, id, nullptr});
  }
}

Size Window::getSize() const {
  return d->platform_->currentSize();
}

void Window::setTitle(std::string title) {
  d->platform_->setTitle(std::move(title));
}

void Window::setFullscreen(bool fullscreen) {
  d->platform_->setFullscreen(fullscreen);
}

unsigned int Window::handle() const {
  return d->platform_->handle();
}

Canvas& Window::canvas() {
  if (!d->canvas_) {
    d->canvas_ = d->platform_->createCanvas(*this);
  }
  return *d->canvas_;
}

bool Window::hasSceneGraph() const { return d->sceneGraph_.has_value(); }

SceneGraph& Window::sceneGraph() {
  if (!d->sceneGraph_) {
    d->sceneGraph_.emplace();
  }
  return *d->sceneGraph_;
}

SceneGraph const& Window::sceneGraph() const { return const_cast<Window*>(this)->sceneGraph(); }

void Window::requestRedraw() { postRedraw(handle()); }

void Window::setCursor(Cursor kind) {
  d->platform_->setCursor(kind);
}

PlatformWindow* Window::platformWindow() const {
  return d->platformWindow();
}

void Window::postRedraw(unsigned int handle) {
  (void)handle;
  if (!Application::hasInstance()) {
    return;
  }
  Application::instance().requestRedraw();
}

void Window::setClearColor(Color color) { d->clearColor_ = color; }

Color Window::clearColor() const { return d->clearColor_; }

OverlayId Window::pushOverlay(Element content, OverlayConfig config) {
  if (!d) {
    return kInvalidOverlayId;
  }
  Runtime* rt = d->runtime_.get();
  return d->overlayMgr_.push(std::move(content), std::move(config), rt);
}

void Window::removeOverlay(OverlayId id) {
  if (!d) {
    return;
  }
  Runtime* rt = d->runtime_.get();
  d->overlayMgr_.remove(id, rt);
}

void Window::clearOverlays() {
  if (!d) {
    return;
  }
  Runtime* rt = d->runtime_.get();
  d->overlayMgr_.clear(rt);
}

OverlayManager& Window::overlayManager() { return d->overlayMgr_; }

OverlayManager const& Window::overlayManager() const { return d->overlayMgr_; }

void Window::setViewRoot(std::unique_ptr<RootHolder> holder) {
  d->setViewRoot(*this, std::move(holder));
}

void Window::render(Canvas& canvas) {
  if (d->sceneGraph_) {
    SceneRenderer{}.render(*d->sceneGraph_, canvas, d->clearColor_);
  }
  for (std::unique_ptr<OverlayEntry> const& up : d->overlayMgr_.entries()) {
    OverlayEntry const& entry = *up;
    canvas.save();
    canvas.transform(Mat3::translate(Point{entry.resolvedFrame.x, entry.resolvedFrame.y}));
    SceneRenderer{}.render(entry.graph, canvas, Colors::transparent);
    canvas.restore();
  }
}

} // namespace flux
