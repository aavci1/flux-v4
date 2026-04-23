#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/PathNode.hpp>

#include <Flux/Graphics/TextSystem.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/Layout/Algorithms/OverlayLayout.hpp"

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
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
  LocalId const contentLocal = LocalId::fromString("$content");
  PopoverCalloutPlan const plan =
      planPopoverCalloutLayout(callout, ctx.innerConstraints(), [&](LayoutConstraints const& contentConstraints) {
        return ctx.measureChild(callout.content, contentLocal, contentConstraints, LayoutHints{});
      });

  auto group = std::make_unique<scenegraph::GroupNode>(build::sizeRect(plan.calloutLayout.totalSize));
  auto chromeNode = std::make_unique<scenegraph::PathNode>(
      Rect {
          0.f,
          0.f,
          plan.calloutLayout.totalSize.width,
          plan.calloutLayout.totalSize.height,
      },
      plan.calloutLayout.chromePath,
      FillStyle::solid(callout.backgroundColor),
      StrokeStyle::solid(callout.borderColor, callout.borderWidth),
      ShadowStyle::none()
  );

  ctx.recordMeasuredSize(callout.content, contentLocal, plan.contentConstraints, LayoutHints{}, plan.contentMeasured);
  std::unique_ptr<scenegraph::SceneNode> contentNode =
      ctx.buildChild(callout.content, contentLocal, plan.contentConstraints, LayoutHints{},
                     Point{ctx.contentOrigin().x + plan.calloutLayout.contentOrigin.x,
                           ctx.contentOrigin().y + plan.calloutLayout.contentOrigin.y},
                     plan.contentMeasured, true, true);
  contentNode->setPosition(plan.calloutLayout.contentOrigin);

  std::vector<std::unique_ptr<scenegraph::SceneNode>> nextChildren{};
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
