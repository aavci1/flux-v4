#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>

#include <Flux/Graphics/TextSystem.hpp>

#include "UI/Layout/Algorithms/OverlayLayout.hpp"

namespace flux {

Size PopoverCalloutShape::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                                  TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutConstraints cc = layout::innerConstraintsForPopoverContent(*this, constraints);

  ctx.pushChildIndex();
  Size const inner = content.measure(ctx, cc, LayoutHints{}, ts);
  ctx.popChildIndex();
  return layout::layoutPopoverCallout(*this, inner, constraints).totalSize;
}

} // namespace flux
