#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/MeasureContext.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace flux {

namespace {

Size measureChild(Element const& child, MeasureContext& ctx, LayoutConstraints constraints,
                  LayoutHints hints, TextSystem& textSystem) {
  ctx.pushConstraints(constraints, hints);
  Size size = child.measure(ctx, constraints, hints, textSystem);
  ctx.popConstraints();
  return size;
}

float finiteMax(float value) {
  return std::isfinite(value) ? std::max(0.f, value) : 0.f;
}

} // namespace

Size Spacer::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                     LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  return Size{finiteMax(constraints.maxWidth), finiteMax(constraints.maxHeight)};
}

Size VStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                     LayoutHints const&, TextSystem& textSystem) const {
  float width = 0.f;
  float height = 0.f;
  LayoutConstraints childConstraints = constraints;
  childConstraints.minWidth = 0.f;
  childConstraints.minHeight = 0.f;
  childConstraints.maxHeight = std::numeric_limits<float>::infinity();

  LayoutHints childHints{};
  childHints.vStackCrossAlign = alignment;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Size const childSize = measureChild(children[i], ctx, childConstraints, childHints, textSystem);
    width = std::max(width, childSize.width);
    height += childSize.height;
    if (i + 1 < children.size()) {
      height += spacing;
    }
  }
  width = std::max(width, constraints.minWidth);
  height = std::max(height, constraints.minHeight);
  if (std::isfinite(constraints.maxWidth)) {
    width = std::min(width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight)) {
    height = std::min(height, constraints.maxHeight);
  }
  return Size{width, height};
}

Size HStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                     LayoutHints const&, TextSystem& textSystem) const {
  float width = 0.f;
  float height = 0.f;
  LayoutConstraints childConstraints = constraints;
  childConstraints.minWidth = 0.f;
  childConstraints.minHeight = 0.f;
  childConstraints.maxWidth = std::numeric_limits<float>::infinity();

  LayoutHints childHints{};
  childHints.hStackCrossAlign = alignment;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Size const childSize = measureChild(children[i], ctx, childConstraints, childHints, textSystem);
    width += childSize.width;
    height = std::max(height, childSize.height);
    if (i + 1 < children.size()) {
      width += spacing;
    }
  }
  width = std::max(width, constraints.minWidth);
  height = std::max(height, constraints.minHeight);
  if (std::isfinite(constraints.maxWidth)) {
    width = std::min(width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight)) {
    height = std::min(height, constraints.maxHeight);
  }
  return Size{width, height};
}

Size ZStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                     LayoutHints const&, TextSystem& textSystem) const {
  float width = constraints.minWidth;
  float height = constraints.minHeight;
  LayoutHints childHints{};
  childHints.zStackHorizontalAlign = horizontalAlignment;
  childHints.zStackVerticalAlign = verticalAlignment;
  for (Element const& child : children) {
    Size const childSize = measureChild(child, ctx, constraints, childHints, textSystem);
    width = std::max(width, childSize.width);
    height = std::max(height, childSize.height);
  }
  if (std::isfinite(constraints.maxWidth)) {
    width = std::min(width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight)) {
    height = std::min(height, constraints.maxHeight);
  }
  return Size{width, height};
}

} // namespace flux
