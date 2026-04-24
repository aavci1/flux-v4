#include <Flux/UI/CursorController.hpp>

#include <Flux/Core/Window.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
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
                                      scenegraph::SceneGraph const& mainGraph) {
  if (auto const* ps = gesture.press()) {
    auto const [node, pressed] = gesture.findPressInteraction(*ps, overlayEntries, mainGraph);
    (void)node;
    if (pressed && pressed->cursor != Cursor::Inherit) {
      apply(pressed->cursor);
      return;
    }
  }

  for (OverlayEntry const* p : overlayEntries) {
    OverlayEntry const& oe = *p;
    Point const local{ windowPoint.x - oe.resolvedFrame.x, windowPoint.y - oe.resolvedFrame.y };
    if (auto hit = scenegraph::hitTestInteraction(
            oe.sceneGraph, local, [](scenegraph::InteractionData const& interaction) {
          return interaction.cursor != Cursor::Inherit;
        })) {
      apply(hit->interaction->cursor);
      return;
    }
  }

  auto hit = scenegraph::hitTestInteraction(
      mainGraph, windowPoint, [](scenegraph::InteractionData const& interaction) {
    return interaction.cursor != Cursor::Inherit;
  });
  if (hit) {
    apply(hit->interaction->cursor);
  } else {
    apply(Cursor::Arrow);
  }
}

} // namespace flux
