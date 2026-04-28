#include <Flux/UI/Views/Render.hpp>

#include <Flux/SceneGraph/RenderNode.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

namespace {

Size assignedSize(LayoutConstraints const& constraints, Size measured) {
  Size size = measured;
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    size.width = constraints.maxWidth;
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.height = constraints.maxHeight;
  }
  size.width = std::max(size.width, constraints.minWidth);
  size.height = std::max(size.height, constraints.minHeight);
  return Size{std::max(0.f, size.width), std::max(0.f, size.height)};
}

} // namespace

Size Render::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                     LayoutHints const& hints, TextSystem&) const {
  ctx.advanceChildSlot();
  return measure(constraints, hints);
}

std::unique_ptr<scenegraph::SceneNode> Render::mount(MountContext& ctx) const {
  Size const measured = measure(ctx.constraints(), ctx.hints());
  Size const size = assignedSize(ctx.constraints(), measured);
  auto node = std::make_unique<scenegraph::RenderNode>(
      Rect{0.f, 0.f, size.width, size.height}, draw, pure);
  auto* rawNode = node.get();
  auto measure = measureFn;
  LayoutHints hints = ctx.hints();
  rawNode->setLayoutConstraints(ctx.constraints());
  rawNode->setRelayout([rawNode, measure = std::move(measure),
                        hints](LayoutConstraints const& constraints) mutable {
    Size measured{};
    if (measure) {
      measured = measure(constraints, hints);
    }
    Size const nextSize = assignedSize(constraints, measured);
    rawNode->setSize(nextSize);
  });
  return node;
}

} // namespace flux
