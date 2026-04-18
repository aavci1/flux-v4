#include "Core/WindowRender.hpp"

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/Render.hpp>
#include <Flux/Scene/SceneBoundsOverlay.hpp>
#include <Flux/UI/Overlay.hpp>

#include <Flux/Detail/Runtime.hpp>

namespace flux {

void renderWindowFrame(Canvas& canvas, std::optional<SceneTree> const& sceneTree,
                       OverlayManager const& overlays, Runtime const* runtime, Color clearColor,
                       TextCacheRingBuffer& textCacheRing) {
  if (sceneTree) {
    canvas.clear(clearColor);
    flux::render(*sceneTree, canvas);
  }

  for (std::unique_ptr<OverlayEntry> const& up : overlays.entries()) {
    OverlayEntry const& entry = *up;
    canvas.save();
    canvas.transform(Mat3::translate(Point{entry.resolvedFrame.x, entry.resolvedFrame.y}));
    flux::render(entry.sceneTree, canvas);
    canvas.restore();
  }

  if (runtime && runtime->layoutOverlayEnabled()) {
    if (sceneTree) {
      renderSceneBoundsOverlay(*sceneTree, canvas);
    }
    for (std::unique_ptr<OverlayEntry> const& up : overlays.entries()) {
      OverlayEntry const& entry = *up;
      canvas.save();
      canvas.transform(Mat3::translate(Point{entry.resolvedFrame.x, entry.resolvedFrame.y}));
      renderSceneBoundsOverlay(entry.sceneTree, canvas);
      canvas.restore();
    }
  }

  if (runtime && runtime->textCacheOverlayEnabled()) {
    Rect const clipBounds = canvas.clipBounds();
    renderTextCacheDebugOverlay(canvas, clipBounds, textCacheRing);
  }
}

} // namespace flux
