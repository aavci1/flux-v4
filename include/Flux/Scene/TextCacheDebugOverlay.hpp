#pragma once

/// \file Flux/Scene/TextCacheDebugOverlay.hpp
///
/// When the layout debug overlay is enabled (\ref Runtime::setLayoutOverlayEnabled), draws a small
/// "TEXT CACHE" panel with rolling hit rates (last ~60 frames) for each Core Text cache layer.

#include <Flux/Core/Types.hpp>

namespace flux {

class Canvas;

/// Draws the text-cache stats panel in the top-left of the window (call after scene + overlay render).
void renderTextCacheDebugOverlay(Canvas& canvas, Rect viewport);

} // namespace flux
