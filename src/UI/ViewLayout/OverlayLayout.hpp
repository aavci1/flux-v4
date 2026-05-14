#pragma once

#include <Flux/Core/Geometry.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/Views/PopoverCalloutPath.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>

namespace flux::layout {

struct PopoverCalloutLayout {
  Size totalSize{};
  Size contentSize{};
  Rect cardRect{};
  Point contentOrigin{};
  LayoutConstraints contentConstraints{};
  Path chromePath{};
};

LayoutConstraints innerConstraintsForPopoverContent(PopoverCalloutShape const& value,
                                                    LayoutConstraints constraints);

PopoverCalloutLayout layoutPopoverCallout(PopoverCalloutShape const& value, Size contentSize,
                                          LayoutConstraints const& constraints);

Rect resolveOverlayFrame(Size windowSize, OverlayConfig const& config, Rect contentBounds);

} // namespace flux::layout
