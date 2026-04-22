#include <Flux/UI/Views/Render.hpp>
#include <Flux/Scene/RenderSceneNode.hpp>
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
                                            std::unique_ptr<SceneNode> existing) {
  std::unique_ptr<RenderSceneNode> renderNode = build::releaseAs<RenderSceneNode>(std::move(existing));
  if (!renderNode) {
    renderNode = std::make_unique<RenderSceneNode>(ctx.nodeId());
  }
  Rect const frameRect =
      build::assignedFrameForLeaf(ctx.paddedContentSize(), ctx.innerConstraints(), ctx.contentAssignedSize(),
                                  ctx.hasAssignedWidth(), ctx.hasAssignedHeight(), ctx.modifiers(), ctx.hints());
  bool dirty = false;
  dirty |= build::updateIfChanged(renderNode->frame, frameRect);
  if (renderNode->pure != renderView.pure) {
    renderNode->pure = renderView.pure;
    dirty = true;
  }
  if (!renderNode->pure || !renderNode->draw || renderNode->draw.target_type() != renderView.draw.target_type()) {
    renderNode->draw = renderView.draw;
    dirty = true;
  }
  if (dirty) {
    renderNode->invalidatePaint();
    renderNode->markBoundsDirty();
  }
  renderNode->position = {};
  renderNode->recomputeBounds();

  ComponentBuildResult result{};
  result.node = std::move(renderNode);
  result.geometrySize = ctx.layoutOuterSize();
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
