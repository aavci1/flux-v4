#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextCacheDebugOverlay.hpp>

#include <optional>

namespace flux {

class Canvas;
class OverlayManager;
class Runtime;
namespace scenegraph {
class SceneGraph;
}

void renderWindowFrame(Canvas& canvas, std::optional<scenegraph::SceneGraph> const& sceneGraph,
                       OverlayManager const& overlays, Runtime const* runtime, Color clearColor,
                       TextCacheRingBuffer& textCacheRing);

} // namespace flux
