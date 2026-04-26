#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Overlay.hpp>

#include <utility>

namespace flux {

struct Runtime::Impl {
  Window& window;
  std::unique_ptr<MountRoot> root;

  explicit Impl(Window& w) : window(w) {}
};

thread_local Runtime* Runtime::current_ = nullptr;

Runtime::Runtime(Window& window)
    : d(std::make_unique<Impl>(window)) {
  if (Application::hasInstance()) {
    unsigned int const handle = window.handle();
    Application::instance().eventQueue().on<InputEvent>(
        [this, handle](InputEvent const& event) {
          if (event.handle == handle) {
            handleInput(event);
          }
        });
  }
}

Runtime::~Runtime() = default;

void Runtime::setRoot(std::unique_ptr<RootHolder> holder) {
  d->root = std::make_unique<MountRoot>(
      std::move(holder), Application::instance().textSystem(), d->window.environmentLayer(),
      d->window.getSize(), [handle = d->window.handle()] {
        Window::postRedraw(handle);
      });
  Runtime* previous = current_;
  current_ = this;
  d->root->mount(d->window.sceneGraph());
  current_ = previous;
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

Window& Runtime::window() noexcept {
  return d->window;
}

Window const& Runtime::window() const noexcept {
  return d->window;
}

Runtime* Runtime::current() noexcept {
  return current_;
}

namespace {

struct HitTarget {
  scenegraph::InteractionData const* interaction = nullptr;
  Point localPoint{};
  OverlayId overlay{};
};

std::optional<HitTarget> hitOverlay(OverlayEntry const& entry, Point windowPoint) {
  Point const local{windowPoint.x - entry.resolvedFrame.x, windowPoint.y - entry.resolvedFrame.y};
  if (auto hit = scenegraph::hitTestInteraction(entry.sceneGraph, local)) {
    return HitTarget{
        .interaction = hit->interaction,
        .localPoint = hit->localPoint,
        .overlay = entry.id,
    };
  }
  return std::nullopt;
}

std::optional<HitTarget> hitWindow(Window& window, Point point) {
  auto const& overlays = window.overlayManager().entries();
  for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
    if (*it) {
      if (auto hit = hitOverlay(**it, point)) {
        return hit;
      }
    }
  }
  if (window.hasSceneGraph()) {
    if (auto hit = scenegraph::hitTestInteraction(window.sceneGraph(), point)) {
      return HitTarget{
          .interaction = hit->interaction,
          .localPoint = hit->localPoint,
      };
    }
  }
  return std::nullopt;
}

} // namespace

void Runtime::handleInput(InputEvent const& event) {
  if (event.kind == InputEvent::Kind::KeyDown && event.key == keys::Escape) {
    OverlayEntry const* top = d->window.overlayManager().top();
    if (top && top->config.dismissOnEscape) {
      d->window.removeOverlay(top->id);
      return;
    }
  }

  std::optional<HitTarget> hit = hitWindow(d->window, event.position);
  if (!hit || !hit->interaction) {
    return;
  }

  scenegraph::InteractionData const& interaction = *hit->interaction;
  switch (event.kind) {
  case InputEvent::Kind::PointerDown:
    if (interaction.onPointerDown) {
      interaction.onPointerDown(hit->localPoint);
    }
    break;
  case InputEvent::Kind::PointerMove:
    if (interaction.onPointerMove) {
      interaction.onPointerMove(hit->localPoint);
    }
    break;
  case InputEvent::Kind::PointerUp:
    if (interaction.onPointerUp) {
      interaction.onPointerUp(hit->localPoint);
    }
    if (interaction.onTap) {
      interaction.onTap();
    }
    break;
  case InputEvent::Kind::Scroll:
    if (interaction.onScroll) {
      interaction.onScroll(event.scrollDelta);
    }
    break;
  case InputEvent::Kind::KeyDown:
    if (interaction.onKeyDown) {
      interaction.onKeyDown(event.key, event.modifiers);
    }
    break;
  case InputEvent::Kind::KeyUp:
    if (interaction.onKeyUp) {
      interaction.onKeyUp(event.key, event.modifiers);
    }
    break;
  case InputEvent::Kind::TextInput:
    if (interaction.onTextInput) {
      interaction.onTextInput(event.text);
    }
    break;
  case InputEvent::Kind::TouchBegin:
  case InputEvent::Kind::TouchMove:
  case InputEvent::Kind::TouchEnd:
    break;
  }
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
