#pragma once

#include "Compositor/WaylandServer.hpp"

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/TextSystem.hpp>

namespace flux::compositor {

void drawWindowChrome(Canvas& canvas, TextSystem& textSystem, CommittedSurfaceSnapshot const& surface);
void drawSnapPreview(Canvas& canvas, SnapPreviewSnapshot const& preview);

} // namespace flux::compositor
