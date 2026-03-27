#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/Canvas.hpp>

#include "Core/PlatformWindow.hpp"
#include "Core/PlatformWindowCreate.hpp"

namespace flux {

struct Window::Impl {
  std::unique_ptr<PlatformWindow> platform_;
  std::unique_ptr<Canvas> canvas_;
};

Window::Window(const WindowConfig& config) {
  d = std::make_unique<Impl>();
  d->platform_ = detail::createPlatformWindow(config);
  d->platform_->setFluxWindow(this);
  Application::instance().eventQueue().post(WindowLifecycleEvent{
      WindowLifecycleEvent::Kind::Registered,
      handle(),
      this,
  });
  Application::instance().eventQueue().dispatch();
}

Window::~Window() {
  const unsigned int id = handle();
  Application::instance().eventQueue().post(
      WindowLifecycleEvent{WindowLifecycleEvent::Kind::Unregistered, id, nullptr});
  Application::instance().eventQueue().dispatch();
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

void Window::requestRedraw() { postRedraw(handle()); }

void Window::postRedraw(unsigned int handle) {
  Application::instance().eventQueue().post(WindowEvent{
      WindowEvent::Kind::Redraw,
      handle,
      {},
      0.f,
  });
}

void Window::render(Canvas& /*canvas*/) {}

} // namespace flux
