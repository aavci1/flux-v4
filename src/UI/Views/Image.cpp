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
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
  Rect const frameRect =
      build::assignedFrameForLeaf(ctx.paddedContentSize(), ctx.innerConstraints(), ctx.contentAssignedSize(),
                                  ctx.hasAssignedWidth(), ctx.hasAssignedHeight(), ctx.modifiers(), ctx.hints());
  auto imageNode = std::make_unique<scenegraph::ImageNode>(
      Rect {0.f, 0.f, frameRect.width, frameRect.height},
      image.source,
      image.fillMode
  );

  ComponentBuildResult result{};
  result.node = std::move(imageNode);
  result.geometrySize = ctx.layoutOuterSize();
  result.hasGeometrySize = true;
  return result;
}

} // namespace flux::detail
