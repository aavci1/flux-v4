#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/MountRoot.hpp>

#include <utility>

namespace flux {

struct Runtime::Impl {
  Window& window;
  std::unique_ptr<MountRoot> root;

  explicit Impl(Window& w) : window(w) {}
};

Runtime::Runtime(Window& window)
    : d(std::make_unique<Impl>(window)) {}

Runtime::~Runtime() = default;

void Runtime::setRoot(std::unique_ptr<RootHolder> holder) {
  d->root = std::make_unique<MountRoot>(
      std::move(holder), Application::instance().textSystem(), d->window.environmentLayer(),
      d->window.getSize(), [handle = d->window.handle()] {
        Window::postRedraw(handle);
      });
  d->root->mount(d->window.sceneGraph());
  d->window.requestRedraw();
}

void Runtime::beginShutdown() {
  if (d->root && d->window.hasSceneGraph()) {
    d->root->unmount(d->window.sceneGraph());
  }
  d->root.reset();
}

bool Runtime::isActionCurrentlyEnabled(std::string const& name) const {
  (void)name;
  return true;
}

std::optional<Rect> Runtime::layoutRectForKey(ComponentKey const& key) const {
  if (!d->window.hasSceneGraph()) {
    return std::nullopt;
  }
  return d->window.sceneGraph().rectForKey(key);
}

std::optional<Rect> Runtime::layoutRectForLeafKeyPrefix(ComponentKey const& key) const {
  if (!d->window.hasSceneGraph()) {
    return std::nullopt;
  }
  return d->window.sceneGraph().rectForLeafKeyPrefix(key);
}

} // namespace flux
