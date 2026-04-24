#include <Flux/UI/InputDispatcher.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
#include <Flux/SceneGraph/SceneTraversal.hpp>
#include <Flux/UI/BuildOrchestrator.hpp>
#include <Flux/UI/CursorController.hpp>
#include <Flux/UI/FocusController.hpp>
#include <Flux/UI/GestureTracker.hpp>
#include <Flux/UI/HoverController.hpp>
#include <Flux/UI/Overlay.hpp>

#include <cmath>
#include <cstdio>

#include "UI/DebugFlags.hpp"

namespace flux {

namespace {

constexpr float kTapSlop = 8.f;

bool inputDebugEnabled() {
  return debug::inputEnabled();
}

bool inputDebugVerbose() {
  return debug::inputVerbose();
}

bool shouldClaimFocus(scenegraph::InteractionData const& h) { return h.focusable; }

} // namespace

InputDispatcher::InputDispatcher(Window& window, Runtime& runtime, FocusController& focus,
                                   HoverController& hover, GestureTracker& gesture, CursorController& cursor,
                                   BuildOrchestrator& build, bool& windowHasFocus)
    : window_(window)
    , runtime_(runtime)
    , focus_(focus)
    , hover_(hover)
    , gesture_(gesture)
    , cursor_(cursor)
    , build_(build)
    , windowHasFocus_(windowHasFocus) {}

std::vector<OverlayEntry const*> InputDispatcher::overlayEntriesTopFirst() const {
  auto const& entries = window_.overlayManager().entries();
  std::vector<OverlayEntry const*> result;
  result.reserve(entries.size());
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
    result.push_back(it->get());
  }
  return result;
}

std::vector<OverlayEntry const*> InputDispatcher::overlayEntriesBottomFirst() const {
  auto const& entries = window_.overlayManager().entries();
  std::vector<OverlayEntry const*> result;
  result.reserve(entries.size());
  for (auto const& up : entries) {
    result.push_back(up.get());
  }
  return result;
}

void InputDispatcher::dispatch(InputEvent const& e) {
  switch (e.kind) {
  case InputEvent::Kind::KeyDown:
    onKeyDown(e);
    break;
  case InputEvent::Kind::KeyUp:
    onKeyUp(e);
    break;
  case InputEvent::Kind::TextInput:
    onTextInput(e);
    break;
  case InputEvent::Kind::Scroll:
    onScroll(e);
    break;
  case InputEvent::Kind::PointerDown:
    onPointerDown(e);
    break;
  case InputEvent::Kind::PointerMove:
    onPointerMove(e);
    break;
  case InputEvent::Kind::PointerUp:
    onPointerUp(e);
    break;
  default:
    break;
  }
}

void InputDispatcher::onKeyDown(InputEvent const& e) {
  bool const dbg = inputDebugEnabled();

  if (dbg) {
    std::fprintf(stderr, "[flux:input] KeyDown key=%u modifiers=%u\n", static_cast<unsigned>(e.key),
                 static_cast<unsigned>(e.modifiers));
  }
  if (windowHasFocus_ && e.key == keys::L &&
      (e.modifiers & (Modifiers::Meta | Modifiers::Shift)) == (Modifiers::Meta | Modifiers::Shift)) {
    runtime_.setLayoutOverlayEnabled(!runtime_.layoutOverlayEnabled());
    window_.requestRedraw();
    return;
  }
  if (window_.overlayManager().hasOverlays()) {
    OverlayEntry const* top = window_.overlayManager().top();
    if (top->config.modal) {
      if (e.key == keys::Escape && top->config.dismissOnEscape) {
        window_.removeOverlay(top->id);
        return;
      }
      if (e.key == keys::Tab && windowHasFocus_) {
        bool const reverse = any(e.modifiers & Modifiers::Shift);
        focus_.cycleInTree(top->sceneGraph, reverse, top->id);
        return;
      }
      if (!focus_.focusedKey().empty() && windowHasFocus_) {
        auto const [node, interaction] = scenegraph::findInteractionByKey(top->sceneGraph, focus_.focusedKey());
        (void)node;
        if (interaction && interaction->onKeyDown) {
          interaction->onKeyDown(e.key, e.modifiers);
        }
      }
      return;
    }
    if (e.key == keys::Escape && top->config.dismissOnEscape) {
      window_.removeOverlay(top->id);
      return;
    }
    if (e.key == keys::Tab && windowHasFocus_) {
      bool const reverse = any(e.modifiers & Modifiers::Shift);
      focus_.cycleNonModal(overlayEntriesBottomFirst(), window_.sceneGraph(), reverse);
      return;
    }
  }
  if (e.key == keys::Tab && windowHasFocus_) {
    bool const reverse = any(e.modifiers & Modifiers::Shift);
    focus_.cycleInTree(window_.sceneGraph(), reverse, std::nullopt);
    return;
  }
  bool shortcutConsumed = false;
  if (windowHasFocus_) {
    shortcutConsumed = build_.actionRegistryCommitted().dispatchShortcut(
        focus_.focusedKey(), e.key, e.modifiers, window_.actionDescriptors());
    if (shortcutConsumed) {
      return;
    }
  }
  if (!focus_.focusedKey().empty() && windowHasFocus_) {
    if (std::optional<OverlayId> overlayScope = focus_.focusInOverlay()) {
      if (OverlayEntry const* entry = window_.overlayManager().find(*overlayScope)) {
        auto const [node, interaction] =
            scenegraph::findInteractionByKey(entry->sceneGraph, focus_.focusedKey());
        (void)node;
        if (interaction && interaction->onKeyDown) {
          interaction->onKeyDown(e.key, e.modifiers);
        }
        return;
      }
    }
    auto const [node, interaction] =
        scenegraph::findInteractionByKey(window_.sceneGraph(), focus_.focusedKey());
    (void)node;
    if (interaction && interaction->onKeyDown) {
      interaction->onKeyDown(e.key, e.modifiers);
    }
  }
}

void InputDispatcher::onKeyUp(InputEvent const& e) {
  bool const dbg = inputDebugEnabled();

  if (dbg) {
    std::fprintf(stderr, "[flux:input] KeyUp key=%u modifiers=%u\n", static_cast<unsigned>(e.key),
                 static_cast<unsigned>(e.modifiers));
  }
  if (window_.overlayManager().hasOverlays()) {
    OverlayEntry const* top = window_.overlayManager().top();
    if (top->config.modal) {
      if (!focus_.focusedKey().empty() && windowHasFocus_) {
        auto const [node, interaction] =
            scenegraph::findInteractionByKey(top->sceneGraph, focus_.focusedKey());
        (void)node;
        if (interaction && interaction->onKeyUp) {
          interaction->onKeyUp(e.key, e.modifiers);
        }
      }
      return;
    }
  }
  if (!focus_.focusedKey().empty() && windowHasFocus_) {
    if (std::optional<OverlayId> overlayScope = focus_.focusInOverlay()) {
      if (OverlayEntry const* entry = window_.overlayManager().find(*overlayScope)) {
        auto const [node, interaction] =
            scenegraph::findInteractionByKey(entry->sceneGraph, focus_.focusedKey());
        (void)node;
        if (interaction && interaction->onKeyUp) {
          interaction->onKeyUp(e.key, e.modifiers);
        }
        return;
      }
    }
    auto const [node, interaction] =
        scenegraph::findInteractionByKey(window_.sceneGraph(), focus_.focusedKey());
    (void)node;
    if (interaction && interaction->onKeyUp) {
      interaction->onKeyUp(e.key, e.modifiers);
    }
  }
}

void InputDispatcher::onTextInput(InputEvent const& e) {
  bool const dbg = inputDebugEnabled();

  if (dbg) {
    std::fprintf(stderr, "[flux:input] TextInput len=%zu\n", e.text.size());
  }
  if (window_.overlayManager().hasOverlays()) {
    OverlayEntry const* top = window_.overlayManager().top();
    if (top->config.modal) {
      if (!focus_.focusedKey().empty() && windowHasFocus_) {
        auto const [node, interaction] =
            scenegraph::findInteractionByKey(top->sceneGraph, focus_.focusedKey());
        (void)node;
        if (interaction && interaction->onTextInput) {
          interaction->onTextInput(e.text);
        }
      }
      return;
    }
  }
  if (!focus_.focusedKey().empty() && windowHasFocus_) {
    if (std::optional<OverlayId> overlayScope = focus_.focusInOverlay()) {
      if (OverlayEntry const* entry = window_.overlayManager().find(*overlayScope)) {
        auto const [node, interaction] =
            scenegraph::findInteractionByKey(entry->sceneGraph, focus_.focusedKey());
        (void)node;
        if (interaction && interaction->onTextInput) {
          interaction->onTextInput(e.text);
        }
        return;
      }
    }
    auto const [node, interaction] =
        scenegraph::findInteractionByKey(window_.sceneGraph(), focus_.focusedKey());
    (void)node;
    if (interaction && interaction->onTextInput) {
      interaction->onTextInput(e.text);
    }
  }
}

void InputDispatcher::onScroll(InputEvent const& e) {
  scenegraph::SceneGraph const& graph = window_.sceneGraph();
  bool const dbg = inputDebugEnabled();

  Point const p{ e.position.x, e.position.y };
  Vec2 delta = e.scrollDelta;
  if (!e.preciseScrollDelta) {
    constexpr float kLineHeight = 40.f;
    delta.x *= kLineHeight;
    delta.y *= kLineHeight;
  }
  std::vector<std::unique_ptr<OverlayEntry>> const& oentries = window_.overlayManager().entries();
  for (auto it = oentries.rbegin(); it != oentries.rend(); ++it) {
    OverlayEntry const& oe = **it;
    Point const pl{ p.x - oe.resolvedFrame.x, p.y - oe.resolvedFrame.y };
    if (auto hit = scenegraph::hitTestInteraction(
            oe.sceneGraph, pl, [](scenegraph::InteractionData const& interaction) {
              return static_cast<bool>(interaction.onScroll);
            })) {
      if (hit->interaction && hit->interaction->onScroll) {
        hit->interaction->onScroll(delta);
      }
      return;
    }
  }
  if (dbg) {
    std::fprintf(stderr,
                 "[flux:input] Scroll pos=(%.1f,%.1f) delta=(%.2f,%.2f)\n",
                 static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(delta.x),
                 static_cast<double>(delta.y));
  }
  if (auto hit = scenegraph::hitTestInteraction(
          graph, p, [](scenegraph::InteractionData const& interaction) {
            return static_cast<bool>(interaction.onScroll);
          })) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] Scroll filtered (has onScroll): local=(%.1f,%.1f)\n",
                   static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
    }
    if (hit->interaction && hit->interaction->onScroll) {
      hit->interaction->onScroll(delta);
      if (dbg) {
        std::fprintf(stderr, "[flux:input] onScroll invoked\n");
      }
    }
  } else if (dbg) {
    std::fprintf(stderr, "[flux:input] Scroll: no node with onScroll under cursor\n");
  }
}

void InputDispatcher::onPointerDown(InputEvent const& e) {
  Point const p{ e.position.x, e.position.y };
  bool const dbg = inputDebugEnabled();
  auto const overlays = overlayEntriesTopFirst();
  scenegraph::SceneGraph const& graph = window_.sceneGraph();

  if (dbg) {
    std::fprintf(stderr, "[flux:input] PointerDown pos=(%.1f,%.1f)\n", static_cast<double>(p.x),
                 static_cast<double>(p.y));
  }
  gesture_.clearPress();

  for (OverlayEntry const* poe : overlays) {
    OverlayEntry const& oe = *poe;
    Point const pl{ p.x - oe.resolvedFrame.x, p.y - oe.resolvedFrame.y };
    if (auto hit = scenegraph::hitTestInteraction(oe.sceneGraph, pl)) {
      if (dbg) {
        std::fprintf(stderr, "[flux:input] PointerDown hit local=(%.1f,%.1f)\n",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
      }
      if (scenegraph::InteractionData const* interaction = hit->interaction) {
        bool const modalOverlay = oe.config.modal;
        OverlayId const overlayId = oe.id;
        ComponentKey const stableTargetKey = interaction->stableTargetKey;
        gesture_.recordPress(interaction->stableTargetKey, p, static_cast<bool>(interaction->onTap), oe.id);
        if (interaction->onPointerDown) {
          interaction->onPointerDown(hit->localPoint);
        }
        if (modalOverlay) {
          if (shouldClaimFocus(*interaction)) {
            focus_.set(stableTargetKey, overlayId, FocusInputKind::Pointer);
          } else if (!stableTargetKey.empty()) {
            if (OverlayEntry const* refreshedOverlay = window_.overlayManager().find(overlayId)) {
              focus_.claimFocusForSubtree(stableTargetKey, refreshedOverlay->sceneGraph, overlayId);
            }
          }
        }
      }
      auto freshOverlays = overlayEntriesTopFirst();
      cursor_.updateForPoint(p, gesture_, freshOverlays, graph);
      hover_.updateForPoint(p, freshOverlays, graph);
      return;
    }
    if (oe.config.modal) {
      auto freshOverlays = overlayEntriesTopFirst();
      cursor_.updateForPoint(p, gesture_, freshOverlays, graph);
      hover_.updateForPoint(p, freshOverlays, graph);
      return;
    }
  }
  if (window_.overlayManager().hasOverlays()) {
    OverlayEntry const* top = window_.overlayManager().top();
    if (top && !top->config.modal && top->config.dismissOnOutsideTap) {
      window_.removeOverlay(top->id);
      // `overlays` was built before removal; the dismissed entry is freed — no overlays left for
      // hit-testing in this path (same as resolving only the main tree).
      std::vector<OverlayEntry const*> const noOverlays{};
      cursor_.updateForPoint(p, gesture_, noOverlays, graph);
      hover_.updateForPoint(p, noOverlays, graph);
      return;
    }
  }
  if (auto hit = scenegraph::hitTestInteraction(graph, p)) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerDown hit local=(%.1f,%.1f)\n",
                   static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
    }
    if (scenegraph::InteractionData const* interaction = hit->interaction) {
      gesture_.recordPress(interaction->stableTargetKey, p, static_cast<bool>(interaction->onTap),
                           std::nullopt);
      if (interaction->onPointerDown) {
        interaction->onPointerDown(hit->localPoint);
      }
      if (shouldClaimFocus(*interaction)) {
        focus_.set(interaction->stableTargetKey, std::nullopt, FocusInputKind::Pointer);
      } else if (!interaction->stableTargetKey.empty()) {
        focus_.claimFocusForSubtree(interaction->stableTargetKey, graph, std::nullopt);
      }
    }
  } else if (dbg) {
    std::fprintf(stderr, "[flux:input] PointerDown: no interactive node under cursor\n");
  }
  auto freshOverlays = overlayEntriesTopFirst();
  cursor_.updateForPoint(p, gesture_, freshOverlays, graph);
  hover_.updateForPoint(p, freshOverlays, graph);
}

void InputDispatcher::onPointerMove(InputEvent const& e) {
  Point const p{ e.position.x, e.position.y };
  scenegraph::SceneGraph const& graph = window_.sceneGraph();
  auto const overlays = overlayEntriesTopFirst();
  bool const dbg = inputDebugEnabled();
  bool const dbgMove = dbg && inputDebugVerbose();

  static int moveLogCounter = 0;

  bool const logThisMove = dbgMove || (dbg && (++moveLogCounter % 15 == 0));
  if (logThisMove) {
    std::fprintf(stderr, "[flux:input] PointerMove pos=(%.1f,%.1f) activePress=%s\n",
                 static_cast<double>(p.x), static_cast<double>(p.y), gesture_.press() ? "yes" : "no");
  }
  if (gesture_.press()) {
    float const dx = p.x - gesture_.press()->downPoint.x;
    float const dy = p.y - gesture_.press()->downPoint.y;
    if (dx * dx + dy * dy > kTapSlop * kTapSlop) {
      gesture_.markCancelled();
    }
  }
  if (gesture_.press()) {
    auto const [pressNode, pressed] = gesture_.findPressInteraction(*gesture_.press(), overlays, graph);
    if (pressed && pressed->onPointerMove && pressNode) {
      if (scenegraph::SceneGraph const* pressGraph =
              gesture_.sceneGraphForPress(*gesture_.press(), overlays, graph)) {
        Point rootPoint = p;
        if (OverlayEntry const* overlay = gesture_.overlayForPress(*gesture_.press(), overlays)) {
          rootPoint.x -= overlay->resolvedFrame.x;
          rootPoint.y -= overlay->resolvedFrame.y;
        }
        if (auto local = scenegraph::localPointForNode(pressGraph->root(), rootPoint, pressNode)) {
          if (logThisMove) {
            std::fprintf(stderr, "[flux:input] PointerMove routed to press target local=(%.1f,%.1f)\n",
                         static_cast<double>(local->x), static_cast<double>(local->y));
          }
          pressed->onPointerMove(*local);
          auto freshOverlays = overlayEntriesTopFirst();
          cursor_.updateForPoint(p, gesture_, freshOverlays, graph);
          hover_.updateForPoint(p, freshOverlays, graph);
          return;
        }
      }
      if (dbg) {
        std::fprintf(stderr,
                     "[flux:input] PointerMove: localPointForNode FAILED for active press (bad tree?)\n");
      }
    }
  }
  for (OverlayEntry const* poe : overlays) {
    OverlayEntry const& oe = *poe;
    Point const pl{ p.x - oe.resolvedFrame.x, p.y - oe.resolvedFrame.y };
    if (auto hit = scenegraph::hitTestInteraction(oe.sceneGraph, pl)) {
      if (logThisMove) {
        std::fprintf(stderr, "[flux:input] PointerMove hit-test path local=(%.1f,%.1f)\n",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
      }
      if (hit->interaction && hit->interaction->onPointerMove) {
        hit->interaction->onPointerMove(hit->localPoint);
      }
      auto freshOverlays = overlayEntriesTopFirst();
      cursor_.updateForPoint(p, gesture_, freshOverlays, graph);
      hover_.updateForPoint(p, freshOverlays, graph);
      return;
    }
  }
  if (auto hit = scenegraph::hitTestInteraction(graph, p)) {
    if (logThisMove) {
      std::fprintf(stderr, "[flux:input] PointerMove hit-test path local=(%.1f,%.1f)\n",
                   static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
    }
    if (hit->interaction && hit->interaction->onPointerMove) {
      hit->interaction->onPointerMove(hit->localPoint);
    }
  } else if (logThisMove) {
    std::fprintf(stderr, "[flux:input] PointerMove: no interactive node under cursor\n");
  }
  auto freshOverlays = overlayEntriesTopFirst();
  cursor_.updateForPoint(p, gesture_, freshOverlays, graph);
  hover_.updateForPoint(p, freshOverlays, graph);
}

void InputDispatcher::onPointerUp(InputEvent const& e) {
  Point const p{ e.position.x, e.position.y };
  scenegraph::SceneGraph const& graph = window_.sceneGraph();
  auto const overlays = overlayEntriesTopFirst();
  bool const dbg = inputDebugEnabled();

  if (dbg) {
    std::fprintf(stderr, "[flux:input] PointerUp pos=(%.1f,%.1f)\n", static_cast<double>(p.x),
                 static_cast<double>(p.y));
  }

  std::optional<GestureTracker::PressState> released;
  if (auto const* ps = gesture_.press()) {
    released = *ps;
  }
  gesture_.clearPress();

  bool overlayHandled = false;
  for (OverlayEntry const* poe : overlays) {
    OverlayEntry const& oe = *poe;
    Point const pl{ p.x - oe.resolvedFrame.x, p.y - oe.resolvedFrame.y };
    if (auto hit = scenegraph::hitTestInteraction(oe.sceneGraph, pl)) {
      if (dbg) {
        std::fprintf(stderr,
                     "[flux:input] PointerUp release on interactive node local=(%.1f,%.1f)\n",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
      }
      if (hit->interaction && hit->interaction->onPointerUp) {
        hit->interaction->onPointerUp(hit->localPoint);
      }
      overlayHandled = true;
      break;
    }
  }

  if (!overlayHandled) {
    auto hit = scenegraph::hitTestInteraction(graph, p);
    if (hit) {
      if (dbg) {
        std::fprintf(stderr,
                     "[flux:input] PointerUp release on interactive node local=(%.1f,%.1f)\n",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
      }
      if (hit->interaction && hit->interaction->onPointerUp) {
        hit->interaction->onPointerUp(hit->localPoint);
      }
    } else if (released) {
      if (dbg) {
        std::fprintf(stderr, "[flux:input] PointerUp: no interactive hit; trying press-target up\n");
      }
      auto const [currentNode, pressed] = gesture_.findPressInteraction(*released, overlays, graph);
      if (scenegraph::SceneGraph const* pressGraph =
              gesture_.sceneGraphForPress(*released, overlays, graph)) {
        Point rootPoint = p;
        if (OverlayEntry const* overlay = gesture_.overlayForPress(*released, overlays)) {
          rootPoint.x -= overlay->resolvedFrame.x;
          rootPoint.y -= overlay->resolvedFrame.y;
        }
        if (pressed && pressed->onPointerUp && currentNode) {
          std::optional<Point> local =
              scenegraph::localPointForNode(pressGraph->root(), rootPoint, currentNode);
          if (local) {
            if (dbg) {
              std::fprintf(stderr, "[flux:input] PointerUp fallback local=(%.1f,%.1f)\n",
                           static_cast<double>(local->x), static_cast<double>(local->y));
            }
            pressed->onPointerUp(*local);
          } else {
            pressed->onPointerUp(Point{ 0.f, 0.f });
            if (dbg) {
              std::fprintf(stderr,
                           "[flux:input] PointerUp fallback: localPointForNode FAILED; used (0,0)\n");
            }
          }
        }
      }
    } else if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerUp: no hit and no active press to notify\n");
    }
  }

  auto freshOverlays = overlayEntriesTopFirst();
  if (released) {
    gesture_.dispatchTap(*released, freshOverlays, graph);
    freshOverlays = overlayEntriesTopFirst();
  }
  cursor_.updateForPoint(p, gesture_, freshOverlays, graph);
  hover_.updateForPoint(p, freshOverlays, graph);
}

} // namespace flux
