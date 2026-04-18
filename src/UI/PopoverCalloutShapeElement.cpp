#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>

#include <Flux/Graphics/TextSystem.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

namespace {

LayoutConstraints innerConstraintsForPopoverContent(PopoverCalloutShape const& value, LayoutConstraints cc) {
  if (value.maxSize) {
    if (std::isfinite(value.maxSize->width) && value.maxSize->width > 0.f) {
      cc.maxWidth = std::min(cc.maxWidth, value.maxSize->width);
    }
    if (std::isfinite(value.maxSize->height) && value.maxSize->height > 0.f) {
      cc.maxHeight = std::min(cc.maxHeight, value.maxSize->height);
    }
  }

  float const pad = value.padding;
  float const ah = PopoverCalloutShape::kArrowH;

  float availW = cc.maxWidth;
  float availH = cc.maxHeight;
  if (std::isfinite(availW)) {
    availW -= 2.f * pad;
  }
  if (std::isfinite(availH)) {
    availH -= 2.f * pad;
  }

  if (value.arrow) {
    switch (value.placement) {
    case PopoverPlacement::Below:
    case PopoverPlacement::Above:
      if (std::isfinite(availH)) {
        availH -= ah;
      }
      break;
    case PopoverPlacement::End:
    case PopoverPlacement::Start:
      if (std::isfinite(availW)) {
        availW -= ah;
      }
      break;
    }
  }

  cc.maxWidth = std::max(0.f, availW);
  cc.maxHeight = std::max(0.f, availH);
  return cc;
}

} // namespace

Size PopoverCalloutShape::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                                  TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutConstraints cc = innerConstraintsForPopoverContent(*this, constraints);

  ctx.pushChildIndex();
  Size const inner = content.measure(ctx, cc, LayoutHints{}, ts);
  ctx.popChildIndex();

  float const pad = padding;
  float const ah = PopoverCalloutShape::kArrowH;
  float const awTri = PopoverCalloutShape::kArrowW;

  float const cardW = inner.width + 2.f * pad;
  float const cardH = inner.height + 2.f * pad;

  if (!arrow) {
    return {cardW, cardH};
  }
  switch (placement) {
  case PopoverPlacement::Below:
  case PopoverPlacement::Above:
    return {cardW, cardH + ah};
  case PopoverPlacement::End:
  case PopoverPlacement::Start:
    return {cardW + ah, std::max(cardH, awTri)};
  }
  return {cardW, cardH};
}

} // namespace flux
