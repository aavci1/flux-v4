#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneTraversal.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Overlay.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
#include <utility>

namespace flux {

namespace {

struct RuntimeTargetSnapshot {
  scenegraph::SceneNode const* node = nullptr;
  scenegraph::InteractionData const* interaction = nullptr;
  ComponentKey stableTargetKey{};
  OverlayId overlay{};
  bool inOverlay = false;
  Cursor cursor = Cursor::Inherit;
  bool focusable = false;
  Reactive::SmallFn<void()> onPointerEnter;
  Reactive::SmallFn<void()> onPointerExit;
  Reactive::SmallFn<void()> onFocus;
  Reactive::SmallFn<void()> onBlur;
  Reactive::SmallFn<void(Point)> onPointerDown;
  Reactive::SmallFn<void(Point)> onPointerUp;
  Reactive::SmallFn<void(Point)> onPointerMove;
  Reactive::SmallFn<void(Vec2)> onScroll;
  Reactive::SmallFn<void(KeyCode, Modifiers)> onKeyDown;
  Reactive::SmallFn<void(KeyCode, Modifiers)> onKeyUp;
  Reactive::SmallFn<void(std::string const&)> onTextInput;
  Reactive::SmallFn<void()> onTap;
  Reactive::Signal<bool> hoverSignal;
  Reactive::Signal<bool> pressSignal;
  Reactive::Signal<bool> focusSignal;
  Reactive::Signal<bool> keyboardFocusSignal;
};

struct RuntimeInputState {
  std::vector<RuntimeTargetSnapshot> hoverChain;
  std::optional<RuntimeTargetSnapshot> pressTarget;
  std::optional<RuntimeTargetSnapshot> focusTarget;
  Point pressPoint{};
  bool pressCancelled = false;
  Cursor currentCursor = Cursor::Arrow;
};

} // namespace

struct Runtime::Impl {
  Window& window;
  std::unique_ptr<MountRoot> root;
  ActionRegistry actions;
  RuntimeInputState input;
  std::shared_ptr<bool> alive = std::make_shared<bool>(true);

  explicit Impl(Window& w) : window(w) {}
};

thread_local Runtime* Runtime::current_ = nullptr;

Runtime::Runtime(Window& window)
    : d(std::make_unique<Impl>(window)) {
  if (Application::hasInstance()) {
    unsigned int const handle = window.handle();
    std::shared_ptr<bool> alive = d->alive;
    Application::instance().eventQueue().on<InputEvent>(
        [this, handle, alive](InputEvent const& event) {
          if (*alive && event.handle == handle) {
            handleInput(event);
          }
        });
    Application::instance().eventQueue().on<WindowEvent>(
        [this, handle, alive](WindowEvent const& event) {
          if (*alive && event.handle == handle) {
            handleWindowEvent(event);
          }
        });
  }
}

Runtime::~Runtime() {
  if (d && d->alive) {
    *d->alive = false;
  }
}

void Runtime::setRoot(std::unique_ptr<RootHolder> holder) {
  d->actions.beginRebuild();
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
  beginShutdown(d->window.hasSceneGraph() ? &d->window.sceneGraph() : nullptr);
}

void Runtime::beginShutdown(scenegraph::SceneGraph* sceneGraph) {
  if (d->root && sceneGraph) {
    d->root->unmount(*sceneGraph);
  }
  d->root.reset();
}

bool Runtime::isActionCurrentlyEnabled(std::string const& name) const {
  ComponentKey const focusedKey = d->input.focusTarget
      ? d->input.focusTarget->stableTargetKey
      : ComponentKey{};
  return d->actions.isHandlerEnabled(focusedKey, name, d->window.actionDescriptors());
}

ActionRegistry& Runtime::actionRegistry() noexcept {
  return d->actions;
}

ActionRegistry const& Runtime::actionRegistry() const noexcept {
  return d->actions;
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
  scenegraph::SceneNode const* node = nullptr;
  scenegraph::InteractionData const* interaction = nullptr;
  Point localPoint{};
  OverlayId overlay{};
};

using InteractionFilter = Reactive::SmallFn<bool(scenegraph::InteractionData const&)>;

bool acceptAnyInteraction(scenegraph::InteractionData const&) {
  return true;
}

Point eventPoint(InputEvent const& event) {
  return Point{event.position.x, event.position.y};
}

std::optional<HitTarget> hitOverlay(OverlayEntry const& entry, Point windowPoint,
                                    InteractionFilter const& acceptTarget) {
  Point const local{windowPoint.x - entry.resolvedFrame.x, windowPoint.y - entry.resolvedFrame.y};
  if (auto hit = scenegraph::hitTestInteraction(entry.sceneGraph, local, acceptTarget)) {
    return HitTarget{
        .node = hit->node,
        .interaction = hit->interaction,
        .localPoint = hit->localPoint,
        .overlay = entry.id,
    };
  }
  scenegraph::SceneNode const& root = entry.sceneGraph.root();
  scenegraph::InteractionData const* rootInteraction = root.interaction();
  if (rootInteraction && acceptTarget(*rootInteraction)) {
    Size const rootSize = root.size();
    if (windowPoint.x >= 0.f && windowPoint.y >= 0.f &&
        windowPoint.x <= rootSize.width && windowPoint.y <= rootSize.height) {
      return HitTarget{
          .node = &root,
          .interaction = rootInteraction,
          .localPoint = windowPoint,
          .overlay = entry.id,
      };
    }
  }
  return std::nullopt;
}

std::optional<HitTarget> hitWindow(Window& window, Point point,
                                   InteractionFilter const& acceptTarget = acceptAnyInteraction) {
  auto const& overlays = window.overlayManager().entries();
  for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
    if (*it) {
      if (auto hit = hitOverlay(**it, point, acceptTarget)) {
        return hit;
      }
    }
  }
  if (window.hasSceneGraph()) {
    if (auto hit = scenegraph::hitTestInteraction(window.sceneGraph(), point, acceptTarget)) {
      return HitTarget{
          .node = hit->node,
          .interaction = hit->interaction,
          .localPoint = hit->localPoint,
      };
    }
  }
  return std::nullopt;
}

void appendFocusableTargets(scenegraph::SceneGraph const& graph, OverlayId overlay,
                            std::vector<HitTarget>& out) {
  for (ComponentKey const& key : scenegraph::collectFocusableKeys(graph)) {
    auto const [node, interaction] = scenegraph::findInteractionByKey(graph, key);
    if (node && interaction && interaction->focusable) {
      out.push_back(HitTarget{
          .node = node,
          .interaction = interaction,
          .localPoint = {},
          .overlay = overlay,
      });
    }
  }
}

std::vector<HitTarget> focusableTargets(Window& window) {
  std::vector<HitTarget> targets;
  auto const& overlays = window.overlayManager().entries();
  for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
    if (*it) {
      appendFocusableTargets((*it)->sceneGraph, (*it)->id, targets);
      if ((*it)->config.modal && !targets.empty()) {
        return targets;
      }
    }
  }
  if (window.hasSceneGraph()) {
    appendFocusableTargets(window.sceneGraph(), kInvalidOverlayId, targets);
  }
  return targets;
}

RuntimeTargetSnapshot snapshot(scenegraph::SceneNode const* node,
                               scenegraph::InteractionData const* interaction,
                               OverlayId overlay) {
  RuntimeTargetSnapshot target{};
  target.node = node;
  target.interaction = interaction;
  target.overlay = overlay;
  target.inOverlay = overlay.isValid();
  if (interaction) {
    target.stableTargetKey = interaction->stableTargetKey;
    target.cursor = interaction->cursor;
    target.focusable = interaction->focusable;
    target.onPointerEnter = interaction->onPointerEnter;
    target.onPointerExit = interaction->onPointerExit;
    target.onFocus = interaction->onFocus;
    target.onBlur = interaction->onBlur;
    target.onPointerDown = interaction->onPointerDown;
    target.onPointerUp = interaction->onPointerUp;
    target.onPointerMove = interaction->onPointerMove;
    target.onScroll = interaction->onScroll;
    target.onKeyDown = interaction->onKeyDown;
    target.onKeyUp = interaction->onKeyUp;
    target.onTextInput = interaction->onTextInput;
    target.onTap = interaction->onTap;
    target.hoverSignal = interaction->hoverSignal;
    target.pressSignal = interaction->pressSignal;
    target.focusSignal = interaction->focusSignal;
    target.keyboardFocusSignal = interaction->keyboardFocusSignal;
  }
  return target;
}

RuntimeTargetSnapshot snapshot(HitTarget const& hit) {
  return snapshot(hit.node, hit.interaction, hit.overlay);
}

bool sameTarget(RuntimeTargetSnapshot const& lhs, RuntimeTargetSnapshot const& rhs) {
  return lhs.node == rhs.node && lhs.interaction == rhs.interaction &&
         lhs.overlay == rhs.overlay;
}

bool sameTarget(RuntimeTargetSnapshot const& lhs, HitTarget const& rhs) {
  return lhs.node == rhs.node && lhs.interaction == rhs.interaction &&
         lhs.overlay == rhs.overlay;
}

std::vector<RuntimeTargetSnapshot> hoverChainForHit(HitTarget const& hit) {
  std::vector<RuntimeTargetSnapshot> chain;
  for (scenegraph::SceneNode const* node = hit.node; node; node = node->parent()) {
    if (scenegraph::InteractionData const* interaction = node->interaction()) {
      chain.push_back(snapshot(node, interaction, hit.overlay));
    }
  }
  std::reverse(chain.begin(), chain.end());
  return chain;
}

std::optional<Point> localPointForTarget(RuntimeTargetSnapshot const& target,
                                         Window& window, Point windowPoint) {
  if (!target.node) {
    return std::nullopt;
  }
  if (target.inOverlay) {
    OverlayEntry const* entry = window.overlayManager().find(target.overlay);
    if (!entry) {
      return std::nullopt;
    }
    Point const rootPoint{windowPoint.x - entry->resolvedFrame.x,
                          windowPoint.y - entry->resolvedFrame.y};
    return scenegraph::localPointForNode(entry->sceneGraph.root(), rootPoint, target.node);
  }
  if (!window.hasSceneGraph()) {
    return std::nullopt;
  }
  return scenegraph::localPointForNode(window.sceneGraph().root(), windowPoint, target.node);
}

void applyCursor(RuntimeInputState& input, Window& window, Cursor cursor) {
  if (cursor == Cursor::Inherit) {
    cursor = Cursor::Arrow;
  }
  if (input.currentCursor == cursor) {
    return;
  }
  input.currentCursor = cursor;
  window.setCursor(cursor);
}

void writeSignal(Reactive::Signal<bool> const& signal, bool active) {
  if (!signal.disposed()) {
    signal.set(active);
  }
}

void setHoverActive(RuntimeTargetSnapshot const& target, bool active) {
  writeSignal(target.hoverSignal, active);
}

void setPressActive(RuntimeTargetSnapshot const& target, bool active) {
  writeSignal(target.pressSignal, active);
}

void setFocusActive(RuntimeTargetSnapshot const& target, bool active,
                    FocusInputKind kind = FocusInputKind::Pointer) {
  writeSignal(target.focusSignal, active);
  writeSignal(target.keyboardFocusSignal, active && kind == FocusInputKind::Keyboard);
}

void updateCursorForPoint(RuntimeInputState& input, Window& window, Point point) {
  if (input.pressTarget && input.pressTarget->cursor != Cursor::Inherit) {
    applyCursor(input, window, input.pressTarget->cursor);
    return;
  }

  auto hit = hitWindow(window, point, [](scenegraph::InteractionData const& interaction) {
    return interaction.cursor != Cursor::Inherit;
  });
  applyCursor(input, window, hit && hit->interaction ? hit->interaction->cursor : Cursor::Arrow);
}

void updateHoverForPoint(RuntimeInputState& input, Window& window, Point point) {
  std::optional<HitTarget> hit = hitWindow(window, point);
  std::vector<RuntimeTargetSnapshot> nextChain = hit ? hoverChainForHit(*hit)
                                                     : std::vector<RuntimeTargetSnapshot>{};
  if (input.hoverChain.size() == nextChain.size()) {
    bool same = true;
    for (std::size_t i = 0; i < nextChain.size(); ++i) {
      same = same && sameTarget(input.hoverChain[i], nextChain[i]);
    }
    if (same) {
      return;
    }
  }

  std::size_t common = 0;
  while (common < input.hoverChain.size() && common < nextChain.size() &&
         sameTarget(input.hoverChain[common], nextChain[common])) {
    ++common;
  }

  for (std::size_t i = input.hoverChain.size(); i > common; --i) {
    setHoverActive(input.hoverChain[i - 1], false);
    Reactive::SmallFn<void()> exit = input.hoverChain[i - 1].onPointerExit;
    if (exit) {
      exit();
    }
  }

  for (std::size_t i = common; i < nextChain.size(); ++i) {
    setHoverActive(nextChain[i], true);
    Reactive::SmallFn<void()> enter = nextChain[i].onPointerEnter;
    if (enter) {
      enter();
    }
  }

  input.hoverChain = std::move(nextChain);
}

void setFocus(RuntimeInputState& input, std::optional<HitTarget> hit, bool notify = true,
              FocusInputKind kind = FocusInputKind::Pointer) {
  if (input.focusTarget && hit && sameTarget(*input.focusTarget, *hit)) {
    return;
  }

  Reactive::SmallFn<void()> blur;
  if (notify && input.focusTarget) {
    blur = input.focusTarget->onBlur;
  }
  if (input.focusTarget) {
    setFocusActive(*input.focusTarget, false);
  }
  input.focusTarget = hit ? std::optional<RuntimeTargetSnapshot>{snapshot(*hit)}
                         : std::nullopt;
  Reactive::SmallFn<void()> focus;
  if (notify && input.focusTarget) {
    focus = input.focusTarget->onFocus;
  }
  if (input.focusTarget) {
    setFocusActive(*input.focusTarget, true, kind);
  }
  if (blur) {
    blur();
  }
  if (focus) {
    focus();
  }
}

void cycleKeyboardFocus(RuntimeInputState& input, Window& window, bool reverse) {
  std::vector<HitTarget> targets = focusableTargets(window);
  if (targets.empty()) {
    setFocus(input, std::nullopt, true, FocusInputKind::Keyboard);
    return;
  }

  std::size_t nextIndex = reverse ? targets.size() - 1 : 0;
  if (input.focusTarget) {
    for (std::size_t i = 0; i < targets.size(); ++i) {
      if (sameTarget(*input.focusTarget, targets[i])) {
        nextIndex = reverse ? (i == 0 ? targets.size() - 1 : i - 1)
                            : (i + 1) % targets.size();
        break;
      }
    }
  }

  setFocus(input, targets[nextIndex], true, FocusInputKind::Keyboard);
}

ComponentKey focusedActionKey(RuntimeInputState const& input) {
  return input.focusTarget ? input.focusTarget->stableTargetKey : ComponentKey{};
}

void resetTransientTargets(RuntimeInputState& input, Window& window) {
  for (RuntimeTargetSnapshot const& target : input.hoverChain) {
    setHoverActive(target, false);
  }
  if (input.pressTarget) {
    setPressActive(*input.pressTarget, false);
  }
  input.hoverChain.clear();
  input.pressTarget.reset();
  setFocus(input, std::nullopt, false);
  input.pressCancelled = false;
  applyCursor(input, window, Cursor::Arrow);
}

} // namespace

void Runtime::handleInput(InputEvent const& event) {
  Point const point = eventPoint(event);

  if (event.kind == InputEvent::Kind::KeyDown && event.key == keys::Escape) {
    OverlayEntry const* top = d->window.overlayManager().top();
    if (top && top->config.dismissOnEscape) {
      d->window.removeOverlay(top->id);
      return;
    }
  }

  if (event.kind == InputEvent::Kind::KeyDown && event.key == keys::Tab) {
    cycleKeyboardFocus(d->input, d->window, any(event.modifiers & Modifiers::Shift));
    return;
  }

  if (event.kind == InputEvent::Kind::KeyDown &&
      d->actions.dispatchShortcut(focusedActionKey(d->input), event.key, event.modifiers,
                                  d->window.actionDescriptors())) {
    return;
  }

  if (event.kind == InputEvent::Kind::KeyDown ||
      event.kind == InputEvent::Kind::KeyUp ||
      event.kind == InputEvent::Kind::TextInput) {
    if (d->input.focusTarget) {
      switch (event.kind) {
      case InputEvent::Kind::KeyDown:
        if (d->input.focusTarget->onKeyDown) {
          d->input.focusTarget->onKeyDown(event.key, event.modifiers);
        }
        return;
      case InputEvent::Kind::KeyUp:
        if (d->input.focusTarget->onKeyUp) {
          d->input.focusTarget->onKeyUp(event.key, event.modifiers);
        }
        return;
      case InputEvent::Kind::TextInput:
        if (d->input.focusTarget->onTextInput) {
          d->input.focusTarget->onTextInput(event.text);
        }
        return;
      default:
        break;
      }
    }

    std::optional<HitTarget> hit = hitWindow(d->window, point);
    if (!hit || !hit->interaction) {
      return;
    }
    scenegraph::InteractionData const& interaction = *hit->interaction;
    switch (event.kind) {
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
    default:
      break;
    }
    return;
  }

  switch (event.kind) {
  case InputEvent::Kind::PointerDown:
    if (d->input.pressTarget) {
      setPressActive(*d->input.pressTarget, false);
    }
    d->input.pressTarget.reset();
    d->input.pressCancelled = false;
    d->input.pressPoint = point;
    if (auto hit = hitWindow(d->window, point)) {
      RuntimeTargetSnapshot target = snapshot(*hit);
      if (target.focusable) {
        setFocus(d->input, hit, true, FocusInputKind::Pointer);
      } else {
        setFocus(d->input, std::nullopt);
      }
      d->input.pressTarget = target;
      setPressActive(target, true);
      if (target.onPointerDown) {
        target.onPointerDown(hit->localPoint);
      }
    } else {
      setFocus(d->input, std::nullopt);
    }
    updateHoverForPoint(d->input, d->window, point);
    updateCursorForPoint(d->input, d->window, point);
    break;
  case InputEvent::Kind::PointerMove:
    if (d->input.pressTarget) {
      float const dx = point.x - d->input.pressPoint.x;
      float const dy = point.y - d->input.pressPoint.y;
      constexpr float kTapSlop = 8.f;
      if (dx * dx + dy * dy > kTapSlop * kTapSlop) {
        d->input.pressCancelled = true;
      }
      if (d->input.pressTarget->onPointerMove) {
        Point const local = localPointForTarget(*d->input.pressTarget, d->window, point)
                                .value_or(point);
        d->input.pressTarget->onPointerMove(local);
      }
    } else if (auto hit = hitWindow(d->window, point)) {
      if (hit->interaction && hit->interaction->onPointerMove) {
        hit->interaction->onPointerMove(hit->localPoint);
      }
    }
    updateHoverForPoint(d->input, d->window, point);
    updateCursorForPoint(d->input, d->window, point);
    break;
  case InputEvent::Kind::PointerUp:
    if (d->input.pressTarget) {
      RuntimeTargetSnapshot released = *d->input.pressTarget;
      bool const cancelled = d->input.pressCancelled;
      d->input.pressTarget.reset();
      d->input.pressCancelled = false;
      setPressActive(released, false);
      if (released.onPointerUp) {
        Point const local = localPointForTarget(released, d->window, point).value_or(point);
        released.onPointerUp(local);
      }
      if (!cancelled && released.onTap) {
        released.onTap();
      }
    } else if (auto hit = hitWindow(d->window, point)) {
      if (hit->interaction && hit->interaction->onPointerUp) {
        hit->interaction->onPointerUp(hit->localPoint);
      }
      if (hit->interaction && hit->interaction->onTap) {
        hit->interaction->onTap();
      }
    }
    updateHoverForPoint(d->input, d->window, point);
    updateCursorForPoint(d->input, d->window, point);
    break;
  case InputEvent::Kind::Scroll:
    if (auto hit = hitWindow(d->window, point, [](scenegraph::InteractionData const& interaction) {
          return static_cast<bool>(interaction.onScroll);
        })) {
      Vec2 delta = event.scrollDelta;
      if (!event.preciseScrollDelta) {
        constexpr float kLineHeight = 40.f;
        delta.x *= kLineHeight;
        delta.y *= kLineHeight;
      }
      if (hit->interaction && hit->interaction->onScroll) {
        hit->interaction->onScroll(delta);
      }
    }
    break;
  case InputEvent::Kind::KeyDown:
  case InputEvent::Kind::KeyUp:
  case InputEvent::Kind::TextInput:
    break;
  case InputEvent::Kind::TouchBegin:
  case InputEvent::Kind::TouchMove:
  case InputEvent::Kind::TouchEnd:
    break;
  }
}

void Runtime::handleWindowEvent(WindowEvent const& event) {
  switch (event.kind) {
  case WindowEvent::Kind::Resize:
    if (d->root && d->window.hasSceneGraph()) {
      resetTransientTargets(d->input, d->window);
      Runtime* previous = current_;
      current_ = this;
      d->root->resize(event.size, d->window.sceneGraph());
      current_ = previous;
      d->window.overlayManager().rebuild(event.size, *this);
      d->window.requestRedraw();
    }
    break;
  case WindowEvent::Kind::FocusLost:
    resetTransientTargets(d->input, d->window);
    break;
  case WindowEvent::Kind::FocusGained:
  case WindowEvent::Kind::DpiChanged:
  case WindowEvent::Kind::CloseRequest:
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
