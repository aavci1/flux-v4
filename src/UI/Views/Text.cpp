#include <Flux/UI/Views/Text.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Theme.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

namespace {

TextLayoutOptions textLayoutOptions(Text const& text) {
  TextLayoutOptions options{};
  options.horizontalAlignment = text.horizontalAlignment;
  options.verticalAlignment = text.verticalAlignment;
  options.wrapping = text.wrapping;
  options.maxLines = text.maxLines;
  options.firstBaselineOffset = text.firstBaselineOffset;
  return options;
}

} // namespace

Size Text::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                   LayoutHints const&, TextSystem& textSystem) const {
  ctx.advanceChildSlot();
  Theme const& theme = useEnvironment<Theme>();
  Font const resolvedFont = resolveFont(font, theme.bodyFont, theme);
  Color const resolvedColor = resolveColor(color.evaluate(), theme.labelColor, theme);
  TextLayoutOptions const options = textLayoutOptions(*this);

  float maxWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  if (wrapping == TextWrapping::NoWrap) {
    maxWidth = 0.f;
  }
  Size size = textSystem.measure(text.evaluate(), resolvedFont, resolvedColor, maxWidth, options);
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    size.width = std::min(size.width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.height = std::min(size.height, constraints.maxHeight);
  }
  size.width = std::max(size.width, constraints.minWidth);
  size.height = std::max(size.height, constraints.minHeight);
  return size;
}

} // namespace flux
