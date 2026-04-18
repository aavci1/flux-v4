#include <Flux/UI/Views/Text.hpp>

#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Views/TextSupport.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

Size Text::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                   TextSystem& textSystem) const {
  ctx.advanceChildSlot();
  (void)hints;
  auto const [resolvedFont, resolvedColor] = text_detail::resolveBodyTextStyle(font, color);
  TextLayoutOptions const options = text_detail::makeTextLayoutOptions(*this);
  float maxWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  if (wrapping == TextWrapping::NoWrap) {
    maxWidth = 0.f;
  }
  Size size = textSystem.measure(text, resolvedFont, resolvedColor, maxWidth, options);
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    size.width = std::min(size.width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.height = std::min(size.height, constraints.maxHeight);
  }
  return size;
}

} // namespace flux
