#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/TextCacheDebugOverlay.hpp>

#include <optional>

namespace flux {

class Canvas;
class OverlayManager;
class Runtime;
class SceneTree;

void renderWindowFrame(Canvas& canvas, std::optional<SceneTree> const& sceneTree,
                       OverlayManager const& overlays, Runtime const* runtime, Color clearColor,
                       TextCacheRingBuffer& textCacheRing);

} // namespace flux
