#include "Core/WindowRender.hpp"

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/UI/Overlay.hpp>

#include <Flux/Detail/Runtime.hpp>

namespace flux {

void renderWindowFrame(scenegraph::SceneRenderer& renderer, Canvas& canvas,
                       std::optional<scenegraph::SceneGraph> const& sceneGraph,
                       Size windowSize, OverlayManager const& overlays, Runtime const* runtime, Color clearColor,
                       TextCacheRingBuffer& textCacheRing) {
  canvas.clear(clearColor);
  if (sceneGraph) {
    renderer.render(*sceneGraph);
  }

  Rect const windowBounds = Rect::sharp(0.f, 0.f, windowSize.width, windowSize.height);
  for (std::unique_ptr<OverlayEntry> const& up : overlays.entries()) {
    OverlayEntry const& entry = *up;
    if (entry.config.backdropBlurRadius > 0.f) {
      canvas.drawBackdropBlur(windowBounds, entry.config.backdropBlurRadius, entry.config.backdropColor);
    }
    canvas.save();
    canvas.transform(Mat3::translate(Point{entry.resolvedFrame.x, entry.resolvedFrame.y}));
    renderer.render(entry.sceneGraph);
    canvas.restore();
  }

  if (runtime && runtime->textCacheOverlayEnabled()) {
    renderTextCacheDebugOverlay(canvas, windowBounds, textCacheRing);
  }
}

} // namespace flux
