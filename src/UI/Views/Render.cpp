#include <Flux/UI/Views/Render.hpp>

#include <Flux/SceneGraph/RenderNode.hpp>
#include <Flux/UI/MeasureContext.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"

namespace flux {

Size Render::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                     TextSystem&) const {
  ctx.advanceChildSlot();
  return measure(constraints, hints);
}

namespace detail {

ComponentBuildResult buildMeasuredComponent(Render const& renderView, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
  Rect const frameRect =
      build::assignedFrameForLeaf(ctx.paddedContentSize(), ctx.innerConstraints(), ctx.contentAssignedSize(),
                                  ctx.hasAssignedWidth(), ctx.hasAssignedHeight(), ctx.modifiers(),
                                  ctx.hints());

  auto renderNode = std::make_unique<scenegraph::RenderNode>(frameRect, renderView.draw, renderView.pure);
  renderNode->setBounds(Rect{0.f, 0.f, frameRect.width, frameRect.height});

  ComponentBuildResult result{};
  result.node = std::move(renderNode);
  result.geometrySize = ctx.layoutOuterSize();
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
