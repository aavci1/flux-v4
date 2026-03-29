#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/Events.hpp>
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

Runtime::Runtime(Window& window) : window_(window) {
  subscribeToRebuild();
  subscribeInput();
  subscribeResize();
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
  if (inputDebugEnabled()) {
    std::fprintf(stderr,
                 "[flux:input] rebuild layout=%.1fx%.1f scene root children (if any) updated\n",
                 static_cast<double>(sz.width), static_cast<double>(sz.height));
  }
  window_.requestRedraw();
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

void Runtime::subscribeResize() {
  unsigned int const hid = window_.handle();
  Application::instance().eventQueue().on<WindowEvent>([this, hid](WindowEvent const& ev) {
    if (ev.handle != hid || ev.kind != WindowEvent::Kind::Resize) {
      return;
    }
    // Use the size captured when the event was posted; re-querying the view during dispatch can
    // differ slightly from bounds updates and cause visible jitter during live resize.
    rebuild(ev.size);
  });
}

void Runtime::handleInput(InputEvent const& e) {
  SceneGraph const& graph = window_.sceneGraph();
  bool const dbg = inputDebugEnabled();
  bool const dbgMove = dbg && inputDebugVerbose();

  if (e.kind == InputEvent::Kind::Scroll) {
    Point const p{e.position.x, e.position.y};
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
                     static_cast<double>(e.scrollDelta.x), static_cast<double>(e.scrollDelta.y),
                     static_cast<double>(front->localPoint.x), static_cast<double>(front->localPoint.y));
        logNodeId("frontmost (any)", front->nodeId);
      } else {
        std::fprintf(stderr,
                     "[flux:input] Scroll pos=(%.1f,%.1f) delta=(%.2f,%.2f) frontmost geom: (no hit)\n",
                     static_cast<double>(p.x), static_cast<double>(p.y),
                     static_cast<double>(e.scrollDelta.x), static_cast<double>(e.scrollDelta.y));
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
        h->onScroll(e.scrollDelta);
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
        ps.downPoint = p;
        ps.cancelled = false;
        ps.hadOnTapOnDown = static_cast<bool>(h->onTap);
        if (ps.hadOnTapOnDown) {
          ps.downTapTargetKey = h->stableTargetKey;
        }
        activePress_ = std::move(ps);
        if (h->onPointerDown) {
          h->onPointerDown(hit->localPoint);
        }
      }
    } else if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerDown: no interactive node under cursor\n");
    }
    return;
  }

  if (e.kind == InputEvent::Kind::PointerMove) {
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
      if (EventHandlers const* pressed = eventMap_.find(activePress_->nodeId);
          pressed && pressed->onPointerMove) {
        if (auto local = tester.localPointForNode(graph, p, activePress_->nodeId)) {
          if (logThisMove) {
            std::fprintf(stderr,
                         "[flux:input] PointerMove routed to press target local=(%.1f,%.1f)\n",
                         static_cast<double>(local->x), static_cast<double>(local->y));
          }
          pressed->onPointerMove(*local);
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
            !released->downTapTargetKey.empty() && h->stableTargetKey == released->downTapTargetKey;
        if (sameNode || retargetAfterRebuild) {
          h->onTap();
        }
      }
    }
  } else if (released && released->nodeId.isValid()) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerUp: no interactive hit; trying press-target up\n");
      logNodeId("  released press", released->nodeId);
    }
    // Release outside any interactive node: still notify the press target (e.g. end drag).
    if (EventHandlers const* pressed = eventMap_.find(released->nodeId)) {
      if (pressed->onPointerUp) {
        if (auto local = tester.localPointForNode(graph, p, released->nodeId)) {
          if (dbg) {
            std::fprintf(stderr,
                         "[flux:input] PointerUp fallback local=(%.1f,%.1f)\n",
                         static_cast<double>(local->x), static_cast<double>(local->y));
          }
          pressed->onPointerUp(*local);
        } else if (dbg) {
          std::fprintf(stderr, "[flux:input] PointerUp fallback: localPointForNode FAILED\n");
        }
      }
    }
  } else if (dbg) {
    std::fprintf(stderr, "[flux:input] PointerUp: no hit and no active press to notify\n");
  }
}

} // namespace flux
