#include <Flux/UI/CursorController.hpp>

#include <Flux/Core/Window.hpp>
#include <Flux/Scene/SceneTreeInteraction.hpp>
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
                                      SceneTree const& mainTree) {
  if (auto const* ps = gesture.press()) {
    auto const [pressId, pressed] = gesture.findPressInteraction(*ps, overlayEntries, mainTree);
    (void)pressId;
    if (pressed && pressed->cursor != Cursor::Inherit) {
      apply(pressed->cursor);
      return;
    }
  }

  for (OverlayEntry const* p : overlayEntries) {
    OverlayEntry const& oe = *p;
    Point const local{ windowPoint.x - oe.resolvedFrame.x, windowPoint.y - oe.resolvedFrame.y };
    if (auto hit = hitTestInteraction(oe.sceneTree, local, [](InteractionData const& interaction) {
          return interaction.cursor != Cursor::Inherit;
        })) {
      apply(hit->interaction->cursor);
      return;
    }
  }

  auto hit = hitTestInteraction(mainTree, windowPoint, [](InteractionData const& interaction) {
    return interaction.cursor != Cursor::Inherit;
  });
  if (hit) {
    apply(hit->interaction->cursor);
  } else {
    apply(Cursor::Arrow);
  }
}

} // namespace flux
