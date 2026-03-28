#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/SceneRenderer.hpp>

#include "Core/PlatformWindow.hpp"
#include "Core/PlatformWindowCreate.hpp"
#include <optional>

namespace flux {

struct Window::Impl {
  std::unique_ptr<PlatformWindow> platform_;
  std::unique_ptr<Canvas> canvas_;
  std::optional<SceneGraph> sceneGraph_;
  Color clearColor_{Colors::lightGray};
  std::unique_ptr<Runtime> runtime_;

  PlatformWindow* platformWindow() const { return platform_.get(); }
  void setViewRoot(Window& window, Element&& root);
};

void Window::Impl::setViewRoot(Window& window, Element&& root) {
  if (!runtime_) {
    runtime_ = std::make_unique<Runtime>(window);
  }
  runtime_->setView(std::move(root));
}

Window::Window(const WindowConfig& config) {
  d = std::make_unique<Impl>();
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

void Window::setViewRoot(Element&& root) {
  d->setViewRoot(*this, std::move(root));
}

void Window::render(Canvas& canvas) {
  if (d->sceneGraph_) {
    SceneRenderer{}.render(*d->sceneGraph_, canvas, d->clearColor_);
  }
}

} // namespace flux
