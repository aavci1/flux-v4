#include <Flux/UI/Views/ScrollView.hpp>

#include <Flux/Reactive/Effect.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/UI/MeasureContext.hpp>

#include "UI/Layout/Algorithms/ScrollLayout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace flux {

namespace {

LayoutConstraints fixedConstraints(Size size) {
  return LayoutConstraints{
      .maxWidth = std::max(0.f, size.width),
      .maxHeight = std::max(0.f, size.height),
      .minWidth = std::max(0.f, size.width),
      .minHeight = std::max(0.f, size.height),
  };
}

Size viewportFromConstraints(ScrollAxis axis, Size contentSize, LayoutConstraints const& constraints) {
  return layout::resolveMeasuredScrollViewSize(axis, contentSize, constraints);
}

Size measureChild(Element const& child, MeasureContext& ctx, LayoutConstraints const& constraints,
                  TextSystem& textSystem) {
  ctx.pushConstraints(constraints, LayoutHints{});
  Size size = child.measure(ctx, constraints, LayoutHints{}, textSystem);
  ctx.popConstraints();
  return size;
}

} // namespace

Point clampScrollOffset(ScrollAxis axis, Point offset, Size const& viewport, Size const& content) {
  return layout::clampScrollOffset(axis, offset, viewport, content);
}

Size OffsetView::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                         LayoutHints const&, TextSystem& textSystem) const {
  LayoutConstraints childConstraints = layout::scrollChildConstraints(axis, constraints, *viewportSize);
  std::vector<Size> childSizes;
  childSizes.reserve(children.size());
  for (Element const& child : children) {
    childSizes.push_back(measureChild(child, ctx, childConstraints, textSystem));
  }
  return layout::scrollContentSize(axis, childSizes);
}

std::unique_ptr<scenegraph::SceneNode> OffsetView::mount(MountContext& ctx) const {
  LayoutConstraints childConstraints = layout::scrollChildConstraints(axis, ctx.constraints(), *viewportSize);
  std::vector<Size> childSizes;
  childSizes.reserve(children.size());
  for (Element const& child : children) {
    childSizes.push_back(measureChild(child, ctx.measureContext(), childConstraints, ctx.textSystem()));
  }
  Size const content = layout::scrollContentSize(axis, childSizes);
  contentSize = content;

  auto group = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, std::max(0.f, content.width), std::max(0.f, content.height)});
  layout::ScrollContentLayout const layoutPlan =
      layout::layoutScrollContent(axis, *viewportSize, offset, childSizes);
  for (std::size_t index = 0; index < children.size(); ++index) {
    MountContext childCtx = ctx.child(fixedConstraints(layoutPlan.slots[index].assignedSize), {});
    auto childNode = children[index].mount(childCtx);
    if (childNode) {
      childNode->setPosition(layoutPlan.slots[index].origin);
      group->appendChild(std::move(childNode));
    }
  }
  return group;
}

Size ScrollView::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                         LayoutHints const&, TextSystem& textSystem) const {
  LayoutConstraints childConstraints = layout::scrollChildConstraints(axis, constraints, Size{});
  std::vector<Size> childSizes;
  childSizes.reserve(children.size());
  for (Element const& child : children) {
    childSizes.push_back(measureChild(child, ctx, childConstraints, textSystem));
  }

  Size const content = layout::scrollContentSize(axis, childSizes);
  Size const viewport = viewportFromConstraints(axis, content, constraints);
  viewportSize = viewport;
  contentSize = content;
  scrollOffset = layout::clampScrollOffset(axis, *scrollOffset, viewport, content);
  return viewport;
}

std::unique_ptr<scenegraph::SceneNode> ScrollView::mount(MountContext& ctx) const {
  LayoutConstraints premeasureConstraints = layout::scrollChildConstraints(axis, ctx.constraints(), Size{});
  std::vector<Size> childSizes;
  childSizes.reserve(children.size());
  for (Element const& child : children) {
    childSizes.push_back(measureChild(child, ctx.measureContext(), premeasureConstraints, ctx.textSystem()));
  }

  Size const content = layout::scrollContentSize(axis, childSizes);
  Size const viewport = viewportFromConstraints(axis, content, ctx.constraints());
  viewportSize = viewport;
  contentSize = content;
  scrollOffset = layout::clampScrollOffset(axis, *scrollOffset, viewport, content);

  auto viewportNode = std::make_unique<scenegraph::RectNode>(Rect{0.f, 0.f, viewport.width, viewport.height});
  viewportNode->setClipsContents(true);

  auto contentGroup = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, std::max(0.f, content.width), std::max(0.f, content.height)});

  layout::ScrollContentLayout initialLayout =
      layout::layoutScrollContent(axis, viewport, Point{}, childSizes);
  for (std::size_t index = 0; index < children.size(); ++index) {
    MountContext childCtx = ctx.child(fixedConstraints(initialLayout.slots[index].assignedSize), {});
    auto childNode = children[index].mount(childCtx);
    if (childNode) {
      childNode->setPosition(initialLayout.slots[index].origin);
      contentGroup->appendChild(std::move(childNode));
    }
  }

  auto* rawContentGroup = contentGroup.get();
  viewportNode->appendChild(std::move(contentGroup));

  State<Point> offsetState = scrollOffset;
  State<Size> viewportState = viewportSize;
  State<Size> contentState = contentSize;
  ScrollAxis const scrollAxis = axis;
  std::vector<Size> sizes = childSizes;
  std::function<void()> requestRedraw = ctx.redrawCallback();
  Reactive::withOwner(ctx.owner(), [rawContentGroup, offsetState, viewportState,
                                    scrollAxis, sizes = std::move(sizes),
                                    requestRedraw = std::move(requestRedraw)]() mutable {
    Reactive::Effect([rawContentGroup, offsetState, viewportState, scrollAxis,
                      sizes, requestRedraw]() mutable {
      layout::ScrollContentLayout plan = layout::layoutScrollContent(
          scrollAxis, viewportState.get(), offsetState.get(), sizes);
      rawContentGroup->setPosition(Point{-plan.clampedOffset.x, -plan.clampedOffset.y});
      if (plan.clampedOffset != offsetState.peek()) {
        offsetState = plan.clampedOffset;
      }
      if (requestRedraw) {
        requestRedraw();
      }
    });
  });

  auto interaction = std::make_unique<scenegraph::InteractionData>();
  interaction->onScroll = [offsetState, viewportState, contentState, scrollAxis](Vec2 delta) {
    Point next = offsetState.get();
    if (scrollAxis == ScrollAxis::Vertical || scrollAxis == ScrollAxis::Both) {
      next.y -= delta.y;
    }
    if (scrollAxis == ScrollAxis::Horizontal || scrollAxis == ScrollAxis::Both) {
      next.x -= delta.x;
    }
    offsetState = layout::clampScrollOffset(scrollAxis, next, viewportState.get(), contentState.get());
  };
  viewportNode->setInteraction(std::move(interaction));

  return viewportNode;
}

} // namespace flux
