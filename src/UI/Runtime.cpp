#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace flux {

void Runtime::setRoot(std::unique_ptr<RootHolder> holder) {
  rootHolder_ = std::move(holder);
  rebuild();
}

namespace {

/// Whole-point layout size avoids subpixel oscillation in `NSView.bounds` during live resize.
Size snapRootLayoutSize(Size s) {
  return {std::max(1.f, std::round(s.width)), std::max(1.f, std::round(s.height))};
}

/// Logical points; matches UIKit default touch slop (roadmap §2).
constexpr float kTapSlop = 8.f;

// Set FLUX_DEBUG_INPUT=1 to log input routing. FLUX_DEBUG_INPUT_VERBOSE=1 also logs every move.
bool envTruthy(char const* v) {
  return v && v[0] != '\0' && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}

bool inputDebugEnabled() {
  static int cached = -1;
  if (cached < 0) {
    cached = envTruthy(std::getenv("FLUX_DEBUG_INPUT")) ? 1 : 0;
  }
  return cached != 0;
}

bool inputDebugVerbose() {
  static int cached = -1;
  if (cached < 0) {
    cached = envTruthy(std::getenv("FLUX_DEBUG_INPUT_VERBOSE")) ? 1 : 0;
  }
  return cached != 0;
}

void logNodeId(char const* label, NodeId id) {
  if (!id.isValid()) {
    std::fprintf(stderr, "[flux:input] %s: (invalid id)\n", label);
  } else {
    std::fprintf(stderr, "[flux:input] %s: index=%u generation=%u\n", label, id.index, id.generation);
  }
}

char const* inputKindName(InputEvent::Kind k) {
  switch (k) {
  case InputEvent::Kind::PointerMove:
    return "PointerMove";
  case InputEvent::Kind::PointerDown:
    return "PointerDown";
  case InputEvent::Kind::PointerUp:
    return "PointerUp";
  case InputEvent::Kind::Scroll:
    return "Scroll";
  case InputEvent::Kind::KeyDown:
    return "KeyDown";
  case InputEvent::Kind::KeyUp:
    return "KeyUp";
  case InputEvent::Kind::TextInput:
    return "TextInput";
  case InputEvent::Kind::TouchBegin:
    return "TouchBegin";
  case InputEvent::Kind::TouchMove:
    return "TouchMove";
  case InputEvent::Kind::TouchEnd:
    return "TouchEnd";
  default:
    return "?";
  }
}

} // namespace

thread_local Runtime* Runtime::sCurrent = nullptr;

namespace {

bool shouldClaimFocus(EventHandlers const& h) { return h.focusable; }

} // namespace

bool Runtime::isFocusInSubtree(ComponentKey const& key) const noexcept {
  if (focusedKey_.empty() || key.size() > focusedKey_.size()) {
    return false;
  }
  return std::equal(key.begin(), key.end(), focusedKey_.begin());
}

void Runtime::setFocus(ComponentKey const& key) {
  if (focusedKey_ == key) {
    return;
  }
  focusedKey_ = key;
  Application::instance().markReactiveDirty();
}

void Runtime::clearFocus() {
  if (focusedKey_.empty()) {
    return;
  }
  focusedKey_.clear();
  Application::instance().markReactiveDirty();
}

void Runtime::cycleTabFocus(bool reverse) {
  auto const& order = eventMap_.focusOrder();
  if (order.empty()) {
    return;
  }

  if (focusedKey_.empty()) {
    setFocus(reverse ? order.back() : order.front());
    return;
  }

  auto it = std::find(order.begin(), order.end(), focusedKey_);
  if (it == order.end()) {
    setFocus(reverse ? order.back() : order.front());
    return;
  }

  if (!reverse) {
    ++it;
    setFocus(it == order.end() ? order.front() : *it);
  } else {
    setFocus(it == order.begin() ? order.back() : *std::prev(it));
  }
}

Runtime::Runtime(Window& window) : window_(window) {
  subscribeToRebuild();
  subscribeInput();
  subscribeWindowEvents();
}

Runtime::~Runtime() {
  if (rebuildHandle_.isValid()) {
    Application::instance().unobserveNextFrame(rebuildHandle_);
  }
}

void Runtime::rebuild(std::optional<Size> sizeOverride) {
  // Only drop an active gesture when the window size changes; rebuilds from reactive/layout
  // animation reuse new NodeIds and would otherwise break PointerUp matching for taps.
  if (sizeOverride.has_value()) {
    activePress_ = std::nullopt;
  }
  sCurrent = this;
  SceneGraph& graph = window_.sceneGraph();
  graph.clear();

  layoutEngine_.resetForBuild();

  stateStore_.beginRebuild();
  StateStore::setCurrent(&stateStore_);

  EventMap newMap;
  BuildContext ctx{graph, newMap, Application::instance().textSystem(), layoutEngine_};
  Size const raw = sizeOverride.value_or(window_.getSize());
  Size const sz = snapRootLayoutSize(raw);
  LayoutConstraints rootCs{};
  rootCs.maxWidth = sz.width;
  rootCs.maxHeight = sz.height;
  ctx.pushConstraints(rootCs);
  if (rootHolder_) {
    rootHolder_->buildInto(ctx);
  }
  ctx.popConstraints();

  StateStore::setCurrent(nullptr);
  stateStore_.endRebuild();

  eventMap_ = std::move(newMap);
  if (!focusedKey_.empty()) {
    auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
    (void)id;
    if (!h) {
      focusedKey_.clear();
    }
  }
  if (inputDebugEnabled()) {
    std::fprintf(stderr,
                 "[flux:input] rebuild layout=%.1fx%.1f scene root children (if any) updated\n",
                 static_cast<double>(sz.width), static_cast<double>(sz.height));
  }
  window_.requestRedraw();
  sCurrent = nullptr;
}

void Runtime::subscribeToRebuild() {
  rebuildHandle_ = Application::instance().onNextFrameNeeded([this] { rebuild(); });
}

void Runtime::subscribeInput() {
  if (inputRegistered_) {
    return;
  }
  inputRegistered_ = true;
  unsigned int const hid = window_.handle();
  Application::instance().eventQueue().on<InputEvent>([this, hid](InputEvent const& e) {
    if (e.handle != hid) {
      if (inputDebugEnabled()) {
        std::fprintf(stderr, "[flux:input] dropped event (handle mismatch): event=%u window=%u kind=%s\n",
                     static_cast<unsigned>(e.handle), static_cast<unsigned>(hid), inputKindName(e.kind));
      }
      return;
    }
    handleInput(e);
  });
}

void Runtime::subscribeWindowEvents() {
  unsigned int const hid = window_.handle();
  Application::instance().eventQueue().on<WindowEvent>([this, hid](WindowEvent const& ev) {
    if (ev.handle != hid) {
      return;
    }
    if (ev.kind == WindowEvent::Kind::Resize) {
      // Use the size captured when the event was posted; re-querying the view during dispatch can
      // differ slightly from bounds updates and cause visible jitter during live resize.
      rebuild(ev.size);
    } else if (ev.kind == WindowEvent::Kind::FocusLost) {
      windowHasFocus_ = false;
      cancelActivePress(Point{});
      currentCursor_ = Cursor::Default;
    } else if (ev.kind == WindowEvent::Kind::FocusGained) {
      windowHasFocus_ = true;
    }
  });
}

std::pair<NodeId, EventHandlers const*> Runtime::findPressHandlersWithNode(PressState const& ps) const {
  if (EventHandlers const* h = eventMap_.find(ps.nodeId)) {
    return {ps.nodeId, h};
  }
  if (!ps.stableTargetKey.empty()) {
    return eventMap_.findWithIdByKey(ps.stableTargetKey);
  }
  return {kInvalidNodeId, nullptr};
}

void Runtime::cancelActivePress(Point windowPoint) {
  if (!activePress_) {
    return;
  }
  SceneGraph const& graph = window_.sceneGraph();
  auto const [currentId, h] = findPressHandlersWithNode(*activePress_);
  if (h && h->onPointerUp && currentId.isValid()) {
    HitTester tester{};
    std::optional<Point> local = tester.localPointForNode(graph, activePress_->downPoint, currentId);
    if (!local) {
      local = tester.localPointForNode(graph, windowPoint, currentId);
    }
    h->onPointerUp(local.value_or(Point{0.f, 0.f}));
  }
  activePress_ = std::nullopt;
}

void Runtime::applyCursor(Cursor kind) {
  if (kind == currentCursor_) {
    return;
  }
  currentCursor_ = kind;
  window_.setCursor(kind);
}

void Runtime::updateCursorForPoint(Point windowPoint) {
  if (activePress_) {
    auto const [pressId, pressed] = findPressHandlersWithNode(*activePress_);
    (void)pressId;
    if (pressed) {
      applyCursor(pressed->cursor);
      return;
    }
  }

  SceneGraph const& graph = window_.sceneGraph();
  auto const acceptFn = [this](NodeId id) {
    EventHandlers const* h = eventMap_.find(id);
    if (!h) {
      return true;
    }
    return !h->cursorPassthrough;
  };
  auto hit = HitTester{}.hitTest(graph, windowPoint, acceptFn);
  if (hit) {
    Cursor kind = Cursor::Default;
    if (EventHandlers const* h = eventMap_.find(hit->nodeId)) {
      kind = h->cursor;
    }
    applyCursor(kind);
  } else {
    applyCursor(Cursor::Default);
  }
}

void Runtime::handleInput(InputEvent const& e) {
  SceneGraph const& graph = window_.sceneGraph();
  bool const dbg = inputDebugEnabled();
  bool const dbgMove = dbg && inputDebugVerbose();

  if (e.kind == InputEvent::Kind::KeyDown) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] KeyDown key=%u modifiers=%u\n", static_cast<unsigned>(e.key),
                   static_cast<unsigned>(e.modifiers));
    }
    if (e.key == keys::Tab && windowHasFocus_) {
      bool const reverse = any(e.modifiers & Modifiers::Shift);
      cycleTabFocus(reverse);
      return;
    }
    if (!focusedKey_.empty() && windowHasFocus_) {
      auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
      (void)id;
      if (h && h->onKeyDown) {
        h->onKeyDown(e.key, e.modifiers);
      }
    }
    return;
  }
  if (e.kind == InputEvent::Kind::KeyUp) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] KeyUp key=%u modifiers=%u\n", static_cast<unsigned>(e.key),
                   static_cast<unsigned>(e.modifiers));
    }
    if (!focusedKey_.empty() && windowHasFocus_) {
      auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
      (void)id;
      if (h && h->onKeyUp) {
        h->onKeyUp(e.key, e.modifiers);
      }
    }
    return;
  }
  if (e.kind == InputEvent::Kind::TextInput) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] TextInput len=%zu\n", e.text.size());
    }
    if (!focusedKey_.empty() && windowHasFocus_) {
      auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
      (void)id;
      if (h && h->onTextInput) {
        h->onTextInput(e.text);
      }
    }
    return;
  }

  if (e.kind == InputEvent::Kind::Scroll) {
    Point const p{e.position.x, e.position.y};
    Vec2 delta = e.scrollDelta;
    if (!e.preciseScrollDelta) {
      constexpr float kLineHeight = 40.f;
      delta.x *= kLineHeight;
      delta.y *= kLineHeight;
    }
    auto const acceptScroll = [this](NodeId id) {
      EventHandlers const* h = eventMap_.find(id);
      return h && h->onScroll;
    };
    HitTester tester{};
    if (dbg) {
      if (auto front = tester.hitTest(graph, p)) {
        std::fprintf(stderr,
                     "[flux:input] Scroll pos=(%.1f,%.1f) delta=(%.2f,%.2f) frontmost geom: local=(%.1f,%.1f) ",
                     static_cast<double>(p.x), static_cast<double>(p.y),
                     static_cast<double>(delta.x), static_cast<double>(delta.y),
                     static_cast<double>(front->localPoint.x), static_cast<double>(front->localPoint.y));
        logNodeId("frontmost (any)", front->nodeId);
      } else {
        std::fprintf(stderr,
                     "[flux:input] Scroll pos=(%.1f,%.1f) delta=(%.2f,%.2f) frontmost geom: (no hit)\n",
                     static_cast<double>(p.x), static_cast<double>(p.y),
                     static_cast<double>(delta.x), static_cast<double>(delta.y));
      }
    }
    if (auto hit = tester.hitTest(graph, p, acceptScroll)) {
      if (dbg) {
        std::fprintf(stderr,
                     "[flux:input] Scroll filtered (has onScroll): local=(%.1f,%.1f) ",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
        logNodeId("scroll target", hit->nodeId);
      }
      if (EventHandlers const* h = eventMap_.find(hit->nodeId); h && h->onScroll) {
        h->onScroll(delta);
        if (dbg) {
          std::fprintf(stderr, "[flux:input] onScroll invoked\n");
        }
      }
    } else if (dbg) {
      std::fprintf(stderr, "[flux:input] Scroll: no node with onScroll under cursor\n");
    }
    return;
  }

  if (e.kind != InputEvent::Kind::PointerDown && e.kind != InputEvent::Kind::PointerUp &&
      e.kind != InputEvent::Kind::PointerMove) {
    return;
  }
  Point const p{e.position.x, e.position.y};
  auto const acceptFn = [this](NodeId id) { return eventMap_.find(id) != nullptr; };

  static int moveLogCounter = 0;

  if (e.kind == InputEvent::Kind::PointerDown) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerDown pos=(%.1f,%.1f)\n", static_cast<double>(p.x),
                   static_cast<double>(p.y));
    }
    auto hit = HitTester{}.hitTest(graph, p, acceptFn);
    activePress_ = std::nullopt;
    if (hit) {
      if (dbg) {
        std::fprintf(stderr,
                     "[flux:input] PointerDown hit local=(%.1f,%.1f) ",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
        logNodeId("press target", hit->nodeId);
      }
      if (EventHandlers const* h = eventMap_.find(hit->nodeId)) {
        PressState ps{};
        ps.nodeId = hit->nodeId;
        ps.stableTargetKey = h->stableTargetKey;
        ps.downPoint = p;
        ps.cancelled = false;
        ps.hadOnTapOnDown = static_cast<bool>(h->onTap);
        activePress_ = std::move(ps);
        if (h->onPointerDown) {
          h->onPointerDown(hit->localPoint);
        }
        if (shouldClaimFocus(*h)) {
          setFocus(h->stableTargetKey);
        }
      }
    } else if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerDown: no interactive node under cursor\n");
    }
    updateCursorForPoint(p);
    return;
  }

  if (e.kind == InputEvent::Kind::PointerMove) {
    if (activePress_ && !(e.pressedButtons & 1)) {
      cancelActivePress(p);
    }
    bool const logThisMove = dbgMove || (dbg && (++moveLogCounter % 15 == 0));
    if (logThisMove) {
      std::fprintf(stderr, "[flux:input] PointerMove pos=(%.1f,%.1f) activePress=%s\n",
                   static_cast<double>(p.x), static_cast<double>(p.y),
                   activePress_ ? "yes" : "no");
      if (activePress_) {
        logNodeId("  active node", activePress_->nodeId);
      }
    }
    if (activePress_) {
      float const dx = p.x - activePress_->downPoint.x;
      float const dy = p.y - activePress_->downPoint.y;
      if (dx * dx + dy * dy > kTapSlop * kTapSlop) {
        activePress_->cancelled = true;
      }
    }
    HitTester tester{};
    if (activePress_) {
      auto const [pressId, pressed] = findPressHandlersWithNode(*activePress_);
      if (pressed && pressed->onPointerMove && pressId.isValid()) {
        if (auto local = tester.localPointForNode(graph, p, pressId)) {
          if (logThisMove) {
            std::fprintf(stderr,
                         "[flux:input] PointerMove routed to press target local=(%.1f,%.1f)\n",
                         static_cast<double>(local->x), static_cast<double>(local->y));
          }
          pressed->onPointerMove(*local);
          updateCursorForPoint(p);
          return;
        }
        if (dbg) {
          std::fprintf(stderr,
                       "[flux:input] PointerMove: localPointForNode FAILED for active press (bad graph?)\n");
        }
      }
    }
    if (auto hit = tester.hitTest(graph, p, acceptFn)) {
      if (logThisMove) {
        std::fprintf(stderr,
                     "[flux:input] PointerMove hit-test path local=(%.1f,%.1f) ",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
        logNodeId("under cursor", hit->nodeId);
      }
      if (EventHandlers const* h = eventMap_.find(hit->nodeId); h && h->onPointerMove) {
        h->onPointerMove(hit->localPoint);
      }
    } else if (logThisMove) {
      std::fprintf(stderr, "[flux:input] PointerMove: no interactive node under cursor\n");
    }
    updateCursorForPoint(p);
    return;
  }

  // PointerUp
  if (dbg) {
    std::fprintf(stderr, "[flux:input] PointerUp pos=(%.1f,%.1f)\n", static_cast<double>(p.x),
                 static_cast<double>(p.y));
  }
  HitTester tester{};
  std::optional<PressState> const released = activePress_;
  activePress_ = std::nullopt;

  auto hit = tester.hitTest(graph, p, acceptFn);
  if (hit) {
    if (dbg) {
      std::fprintf(stderr,
                   "[flux:input] PointerUp release on interactive node local=(%.1f,%.1f) ",
                   static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
      logNodeId("release hit", hit->nodeId);
    }
    if (EventHandlers const* h = eventMap_.find(hit->nodeId)) {
      if (h->onPointerUp) {
        h->onPointerUp(hit->localPoint);
      }
      if (h->onTap && released && !released->cancelled) {
        bool const sameNode = released->nodeId == hit->nodeId;
        bool const retargetAfterRebuild =
            released->hadOnTapOnDown && eventMap_.find(released->nodeId) == nullptr &&
            !released->stableTargetKey.empty() && h->stableTargetKey == released->stableTargetKey;
        if (sameNode || retargetAfterRebuild) {
          h->onTap();
        }
      }
    }
  } else if (released) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerUp: no interactive hit; trying press-target up\n");
      logNodeId("  released press", released->nodeId);
    }
    // Release outside any interactive node: still notify the press target (e.g. end drag).
    auto const [currentId, pressed] = findPressHandlersWithNode(*released);
    if (pressed && pressed->onPointerUp && currentId.isValid()) {
      std::optional<Point> local = tester.localPointForNode(graph, p, currentId);
      if (local) {
        if (dbg) {
          std::fprintf(stderr,
                       "[flux:input] PointerUp fallback local=(%.1f,%.1f)\n",
                       static_cast<double>(local->x), static_cast<double>(local->y));
        }
        pressed->onPointerUp(*local);
      } else {
        pressed->onPointerUp(Point{0.f, 0.f});
        if (dbg) {
          std::fprintf(stderr, "[flux:input] PointerUp fallback: localPointForNode FAILED; used (0,0)\n");
        }
      }
    }
  } else if (dbg) {
    std::fprintf(stderr, "[flux:input] PointerUp: no hit and no active press to notify\n");
  }
  updateCursorForPoint(p);
}

bool useFocus() {
  Runtime* rt = Runtime::current();
  if (!rt) {
    return false;
  }
  StateStore* store = StateStore::current();
  if (!store) {
    return false;
  }
  return rt->isFocusInSubtree(store->currentComponentKey());
}

} // namespace flux
