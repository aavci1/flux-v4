#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>
#include <Flux/Scene/SceneTree.hpp>

#include <Flux/Graphics/TextSystem.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/Layout/Algorithms/OverlayLayout.hpp"
#include "UI/SceneBuilder/NodeReuse.hpp"

namespace flux {

namespace {

struct PopoverCalloutPlan {
  LayoutConstraints contentConstraints{};
  Size contentMeasured{};
  layout::PopoverCalloutLayout calloutLayout{};
};

template<typename MeasureChildFn>
PopoverCalloutPlan planPopoverCalloutLayout(PopoverCalloutShape const& callout,
                                            LayoutConstraints const& constraints, MeasureChildFn&& measureChild) {
  PopoverCalloutPlan plan{};
  plan.contentConstraints = layout::innerConstraintsForPopoverContent(callout, constraints);
  plan.contentMeasured = measureChild(plan.contentConstraints);
  plan.calloutLayout = layout::layoutPopoverCallout(callout, plan.contentMeasured, constraints);
  return plan;
}

} // namespace

Size PopoverCalloutShape::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                                  TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  ctx.pushChildIndex();
  PopoverCalloutPlan const plan =
      planPopoverCalloutLayout(*this, constraints, [&](LayoutConstraints const& contentConstraints) {
        return content.measure(ctx, contentConstraints, LayoutHints{}, ts);
      });
  ctx.popChildIndex();
  return plan.calloutLayout.totalSize;
}

namespace detail {

ComponentBuildResult buildMeasuredComponent(PopoverCalloutShape const& callout, ComponentBuildContext& ctx,
                                            std::unique_ptr<SceneNode> existing) {
  LocalId const contentLocal = LocalId::fromString("$content");
  PopoverCalloutPlan const plan =
      planPopoverCalloutLayout(callout, ctx.innerConstraints(), [&](LayoutConstraints const& contentConstraints) {
        return ctx.measureChild(callout.content, contentLocal, contentConstraints, LayoutHints{});
      });

  std::unique_ptr<SceneNode> group = build::releasePlainGroup(std::move(existing));
  if (!group) {
    group = std::make_unique<SceneNode>(ctx.nodeId());
  }
  ReusableSceneNodes reusable = releaseReusableChildren(*group);

  NodeId const chromeId = SceneTree::childId(ctx.nodeId(), LocalId::fromString("$chrome"));
  std::unique_ptr<PathSceneNode> chromeNode = takeReusableNodeAs<PathSceneNode>(reusable, chromeId);
  if (!chromeNode) {
    chromeNode = std::make_unique<PathSceneNode>(chromeId);
  }
  bool chromeDirty = false;
  chromeDirty |= build::updateIfChanged(chromeNode->path, plan.calloutLayout.chromePath);
  chromeDirty |= build::updateIfChanged(chromeNode->fill, FillStyle::solid(callout.backgroundColor));
  chromeDirty |= build::updateIfChanged(
      chromeNode->stroke, StrokeStyle::solid(callout.borderColor, callout.borderWidth));
  chromeDirty |= build::updateIfChanged(chromeNode->shadow, ShadowStyle::none());
  if (chromeDirty) {
    chromeNode->invalidatePaint();
    chromeNode->markBoundsDirty();
  }
  chromeNode->position = {};
  chromeNode->recomputeBounds();

  std::unique_ptr<SceneNode> reuseContent = takeReusableNode(reusable, ctx.childId(contentLocal));
  ctx.recordMeasuredSize(callout.content, contentLocal, plan.contentConstraints, LayoutHints{}, plan.contentMeasured);
  std::unique_ptr<SceneNode> contentNode =
      ctx.buildChild(callout.content, contentLocal, plan.contentConstraints, LayoutHints{},
                     Point{ctx.contentOrigin().x + plan.calloutLayout.contentOrigin.x,
                           ctx.contentOrigin().y + plan.calloutLayout.contentOrigin.y},
                     plan.contentMeasured, true, true, std::move(reuseContent));
  contentNode->position.x += plan.calloutLayout.contentOrigin.x;
  contentNode->position.y += plan.calloutLayout.contentOrigin.y;

  std::vector<std::unique_ptr<SceneNode>> nextChildren{};
  nextChildren.reserve(2);
  nextChildren.push_back(std::move(chromeNode));
  nextChildren.push_back(std::move(contentNode));
  group->replaceChildren(std::move(nextChildren));
  build::setGroupBounds(*group, plan.calloutLayout.totalSize);

  ComponentBuildResult result{};
  result.node = std::move(group);
  result.geometrySize = plan.calloutLayout.totalSize;
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
