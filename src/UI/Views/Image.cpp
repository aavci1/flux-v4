#include <Flux/UI/Views/Image.hpp>

#include <Flux/UI/MeasureContext.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"

namespace flux::views {

Size Image::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                    TextSystem&) const {
  ctx.advanceChildSlot();
  float const width = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const height = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  return {width, height};
}

} // namespace flux::views

namespace flux::detail {

ComponentBuildResult buildMeasuredComponent(views::Image const& image, ComponentBuildContext& ctx,
                                            std::unique_ptr<SceneNode> existing) {
  std::unique_ptr<ImageSceneNode> imageNode = build::releaseAs<ImageSceneNode>(std::move(existing));
  if (!imageNode) {
    imageNode = std::make_unique<ImageSceneNode>(ctx.nodeId());
  }
  Rect const frameRect =
      build::assignedFrameForLeaf(ctx.paddedContentSize(), ctx.innerConstraints(), ctx.contentAssignedSize(),
                                  ctx.hasAssignedWidth(), ctx.hasAssignedHeight(), ctx.modifiers(), ctx.hints());
  bool dirty = false;
  dirty |= build::updateIfChanged(imageNode->image, image.source);
  dirty |= build::updateIfChanged(imageNode->size, Size{frameRect.width, frameRect.height});
  dirty |= build::updateIfChanged(imageNode->fillMode, image.fillMode);
  dirty |= build::updateIfChanged(imageNode->cornerRadius,
                                  ctx.modifiers() ? ctx.modifiers()->cornerRadius : CornerRadius{});
  dirty |= build::updateIfChanged(imageNode->opacity, ctx.modifiers() ? ctx.modifiers()->opacity : 1.f);
  if (dirty) {
    imageNode->invalidatePaint();
    imageNode->markBoundsDirty();
  }
  imageNode->position = {};
  imageNode->recomputeBounds();

  ComponentBuildResult result{};
  result.node = std::move(imageNode);
  result.geometrySize = ctx.layoutOuterSize();
  result.hasGeometrySize = true;
  return result;
}

} // namespace flux::detail
