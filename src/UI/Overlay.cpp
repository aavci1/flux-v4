#include <Flux/UI/Overlay.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/RectNode.hpp>

#include "UI/Layout/Algorithms/OverlayLayout.hpp"

#include <algorithm>
#include <cassert>
#include <tuple>
#include <utility>

namespace flux {

namespace {

struct EnvironmentScope {
  EnvironmentStack& stack;
  bool pushed = false;

  EnvironmentScope(EnvironmentStack& environment, EnvironmentLayer layer)
      : stack(environment) {
    stack.push(std::move(layer));
    pushed = true;
  }

  ~EnvironmentScope() {
    if (pushed) {
      stack.pop();
    }
  }
};

LayoutConstraints overlayConstraints(Size windowSize, OverlayConfig const& config) {
  LayoutConstraints constraints{};
  constraints.minWidth = 0.f;
  constraints.minHeight = 0.f;
  constraints.maxWidth = windowSize.width;
  constraints.maxHeight = windowSize.height;
  if (config.maxSize) {
    if (config.maxSize->width > 0.f) {
      constraints.maxWidth = std::min(constraints.maxWidth, config.maxSize->width);
    }
    if (config.maxSize->height > 0.f) {
      constraints.maxHeight = std::min(constraints.maxHeight, config.maxSize->height);
    }
  }
  return constraints;
}

std::unique_ptr<scenegraph::InteractionData> makeBackdropInteraction(Window& window,
                                                                     OverlayEntry& entry,
                                                                     bool dismissOnTap) {
  auto interaction = std::make_unique<scenegraph::InteractionData>();
  interaction->cursor = Cursor::Arrow;
  interaction->onScroll = [](Vec2) {};
  if (dismissOnTap) {
    OverlayId const id = entry.id;
    interaction->onTap = [&window, id] {
      window.removeOverlay(id);
    };
  } else {
    interaction->onTap = [] {};
  }
  return interaction;
}

void insertBackdrop(scenegraph::GroupNode& root, OverlayEntry& entry, Size windowSize,
                    Window& window, bool dismissOnTap) {
  float const ox = -entry.resolvedFrame.x;
  float const oy = -entry.resolvedFrame.y;
  auto backdrop = std::make_unique<scenegraph::RectNode>(
      Rect{ox, oy, windowSize.width, windowSize.height},
      FillStyle::solid(entry.config.backdropColor));
  root.appendChild(std::move(backdrop));

  auto capture = std::make_unique<scenegraph::RectNode>(Rect{ox, oy, windowSize.width, windowSize.height});
  capture->setInteraction(makeBackdropInteraction(window, entry, dismissOnTap));
  root.appendChild(std::move(capture));
}

} // namespace

std::tuple<std::function<void(Element, OverlayConfig)>, std::function<void()>, bool> useOverlay() {
  Runtime* runtime = Runtime::current();
  assert(runtime && "useOverlay must be called while mounting a Flux view");

  auto id = std::make_shared<OverlayId>(kInvalidOverlayId);
  Window* window = &runtime->window();

  Reactive::onCleanup([id, window] {
    if (id->isValid()) {
      OverlayId const removeId = *id;
      *id = kInvalidOverlayId;
      window->removeOverlay(removeId);
    }
  });

  auto hide = [id, window] {
    if (!id->isValid()) {
      return;
    }
    OverlayId const removeId = *id;
    *id = kInvalidOverlayId;
    window->removeOverlay(removeId);
  };

  auto show = [id, window](Element element, OverlayConfig config) {
    if (id->isValid()) {
      OverlayId const removeId = *id;
      *id = kInvalidOverlayId;
      window->removeOverlay(removeId);
    }
    *id = window->pushOverlay(std::move(element), std::move(config));
  };

  return {
      std::move(show),
      std::move(hide),
      id->isValid(),
  };
}

void OverlayManager::rebuild(Size windowSize, Runtime& runtime) {
  for (auto& entryPtr : overlays_) {
    OverlayEntry& entry = *entryPtr;
    entry.scope.dispose();
    entry.scope = Reactive::Scope{};
    entry.sceneGraph.releaseRoot();

    std::unique_ptr<scenegraph::SceneNode> contentNode;
    Rect contentBounds{};
    if (entry.content) {
      LayoutConstraints const constraints = overlayConstraints(windowSize, entry.config);
      MeasureContext measureContext{Application::instance().textSystem()};
      EnvironmentStack& environment = EnvironmentStack::current();
      EnvironmentScope const environmentScope{environment, runtime.window().environmentLayer()};
      MountContext context{entry.scope, environment, Application::instance().textSystem(),
                           measureContext, constraints, LayoutHints{}, [handle = runtime.window().handle()] {
                             Window::postRedraw(handle);
                           }, {}, runtime.window().environmentBinding()};

      contentNode = Reactive::withOwner(entry.scope, [&] {
        return entry.content->mount(context);
      });
      if (contentNode) {
        contentBounds = contentNode->bounds();
        if (contentBounds.width <= 0.f || contentBounds.height <= 0.f) {
          contentBounds = Rect{0.f, 0.f, std::max(1.f, contentNode->size().width),
                               std::max(1.f, contentNode->size().height)};
        }
      }
    }

    if (contentBounds.width <= 0.f) {
      contentBounds.width = 1.f;
    }
    if (contentBounds.height <= 0.f) {
      contentBounds.height = 1.f;
    }
    entry.resolvedFrame = layout::resolveOverlayFrame(windowSize, entry.config, contentBounds);

    auto root = std::make_unique<scenegraph::GroupNode>(
        Rect{0.f, 0.f, std::max(windowSize.width, entry.resolvedFrame.width),
             std::max(windowSize.height, entry.resolvedFrame.height)});
    bool capturesBackdrop = false;
    bool dismissBackdropTap = false;
    if (entry.config.modal) {
      insertBackdrop(*root, entry, windowSize, runtime.window(), false);
      capturesBackdrop = true;
    } else if (entry.config.backdropColor.a > 0.001f) {
      insertBackdrop(*root, entry, windowSize, runtime.window(), entry.config.dismissOnOutsideTap);
      capturesBackdrop = true;
      dismissBackdropTap = entry.config.dismissOnOutsideTap;
    }
    if (capturesBackdrop) {
      root->setInteraction(makeBackdropInteraction(runtime.window(), entry, dismissBackdropTap));
    }
    if (contentNode) {
      root->appendChild(std::move(contentNode));
    }
    entry.sceneGraph.setRoot(std::move(root));
  }
}

OverlayId OverlayManager::push(Element content, OverlayConfig config, Runtime* runtime) {
  auto entry = std::make_unique<OverlayEntry>();
  entry->id = OverlayId{nextId_++};
  entry->content.emplace(std::move(content));
  entry->config = std::move(config);
  OverlayId id = entry->id;
  overlays_.push_back(std::move(entry));
  if (runtime) {
    rebuild(runtime->window().getSize(), *runtime);
    runtime->window().requestRedraw();
  } else if (Application::hasInstance()) {
    Application::instance().requestRedraw();
  }
  return id;
}

void OverlayManager::remove(OverlayId id, Runtime* runtime) {
  std::function<void()> onDismiss;
  std::erase_if(overlays_, [&](std::unique_ptr<OverlayEntry> const& entry) {
    if (entry && entry->id == id) {
      onDismiss = entry->config.onDismiss;
      return true;
    }
    return false;
  });
  if (runtime) {
    runtime->window().requestRedraw();
  } else if (Application::hasInstance()) {
    Application::instance().requestRedraw();
  }
  if (onDismiss) {
    onDismiss();
  }
}

void OverlayManager::clear(Runtime* runtime, bool invokeDismissCallbacks) {
  if (invokeDismissCallbacks) {
    for (auto const& entry : overlays_) {
      if (entry && entry->config.onDismiss) {
        entry->config.onDismiss();
      }
    }
  }
  overlays_.clear();
  if (!runtime && !invokeDismissCallbacks) {
    return;
  }
  if (runtime) {
    runtime->window().requestRedraw();
  } else if (Application::hasInstance()) {
    Application::instance().requestRedraw();
  }
}

OverlayEntry const* OverlayManager::top() const {
  return overlays_.empty() ? nullptr : overlays_.back().get();
}

OverlayEntry* OverlayManager::find(OverlayId id) {
  for (auto& entry : overlays_) {
    if (entry && entry->id == id) {
      return entry.get();
    }
  }
  return nullptr;
}

OverlayEntry const* OverlayManager::find(OverlayId id) const {
  for (auto const& entry : overlays_) {
    if (entry && entry->id == id) {
      return entry.get();
    }
  }
  return nullptr;
}

} // namespace flux
