#include "Core/WindowRender.hpp"

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/UI/Overlay.hpp>

#include <Flux/Detail/Runtime.hpp>

namespace flux {

void renderWindowFrame(Canvas& canvas, std::optional<scenegraph::SceneGraph> const& sceneGraph,
                       OverlayManager const& overlays, Runtime const* runtime, Color clearColor,
                       TextCacheRingBuffer& textCacheRing) {
  scenegraph::SceneRenderer renderer {canvas};
  canvas.clear(clearColor);
  if (sceneGraph) {
    renderer.render(*sceneGraph);
  }

  for (std::unique_ptr<OverlayEntry> const& up : overlays.entries()) {
    OverlayEntry const& entry = *up;
    canvas.save();
    canvas.transform(Mat3::translate(Point{entry.resolvedFrame.x, entry.resolvedFrame.y}));
    renderer.render(entry.sceneGraph);
    canvas.restore();
  }

  if (runtime && runtime->textCacheOverlayEnabled()) {
    Rect const clipBounds = canvas.clipBounds();
    renderTextCacheDebugOverlay(canvas, clipBounds, textCacheRing);
  }
}

} // namespace flux
