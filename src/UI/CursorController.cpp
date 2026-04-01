#include <Flux/UI/CursorController.hpp>

#include <Flux/Core/Window.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/UI/GestureTracker.hpp>

namespace flux {

CursorController::CursorController(Window& window) : window_(window) {}

void CursorController::reset() {
  apply(Cursor::Arrow);
}

void CursorController::apply(Cursor kind) {
  if (kind == current_) {
    return;
  }
  current_ = kind;
  window_.setCursor(kind);
}

void CursorController::updateForPoint(Point windowPoint, GestureTracker const& gesture,
                                      std::vector<OverlayEntry const*> const& overlayEntries,
                                      SceneGraph const& mainGraph, EventMap const& mainEventMap) {
  if (auto const* ps = gesture.press()) {
    auto const [pressId, pressed] = gesture.findPressHandlers(*ps, overlayEntries, mainEventMap);
    (void)pressId;
    if (pressed && pressed->cursor != Cursor::Inherit) {
      apply(pressed->cursor);
      return;
    }
  }

  for (OverlayEntry const* p : overlayEntries) {
    OverlayEntry const& oe = *p;
    Point const local{ windowPoint.x - oe.resolvedFrame.x, windowPoint.y - oe.resolvedFrame.y };
    auto const acceptFn = [&oe](NodeId id) {
      EventHandlers const* h = oe.eventMap.find(id);
      if (!h) {
        return false;
      }
      if (h->cursorPassthrough) {
        return false;
      }
      if (h->cursor == Cursor::Inherit) {
        return false;
      }
      return true;
    };
    HitTester tester{};
    if (auto hit = tester.hitTest(oe.graph, local, acceptFn)) {
      if (EventHandlers const* h = oe.eventMap.find(hit->nodeId)) {
        apply(h->cursor);
        return;
      }
    }
  }

  auto const acceptFn = [&mainEventMap](NodeId id) {
    EventHandlers const* h = mainEventMap.find(id);
    if (!h) {
      return false;
    }
    if (h->cursorPassthrough) {
      return false;
    }
    if (h->cursor == Cursor::Inherit) {
      return false;
    }
    return true;
  };
  auto hit = HitTester{}.hitTest(mainGraph, windowPoint, acceptFn);
  if (hit) {
    if (EventHandlers const* h = mainEventMap.find(hit->nodeId)) {
      apply(h->cursor);
    } else {
      apply(Cursor::Arrow);
    }
  } else {
    apply(Cursor::Arrow);
  }
}

} // namespace flux
