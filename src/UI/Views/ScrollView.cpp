#include <Flux/UI/Views/ScrollView.hpp>

#include <Flux/Reactive/Effect.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/UI/Detail/MountPosition.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Theme.hpp>

#include "UI/Layout/Algorithms/ScrollLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
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

Size scrollViewportHintForConstraints(ScrollAxis axis, LayoutConstraints const& constraints, Size viewportHint) {
  switch (axis) {
  case ScrollAxis::Vertical:
    if (viewportHint.width <= 0.f && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
      viewportHint.width = constraints.maxWidth;
    }
    break;
  case ScrollAxis::Horizontal:
    if (viewportHint.height <= 0.f && std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
      viewportHint.height = constraints.maxHeight;
    }
    break;
  case ScrollAxis::Both:
    break;
  }
  return viewportHint;
}

Size measureChild(Element const& child, MeasureContext& ctx, LayoutConstraints const& constraints,
                  TextSystem& textSystem) {
  ctx.pushConstraints(constraints, LayoutHints{});
  Size size = child.measure(ctx, constraints, LayoutHints{}, textSystem);
  ctx.popConstraints();
  return size;
}

Color scrollIndicatorColor(EnvironmentBinding const& environment) {
  Theme const theme = environment.value<ThemeKey>();
  return Color{
      theme.secondaryLabelColor.r,
      theme.secondaryLabelColor.g,
      theme.secondaryLabelColor.b,
      0.55f,
  };
}

void setIndicatorBounds(scenegraph::RectNode& node, layout::ScrollIndicatorMetrics const& metrics) {
  node.setBounds(Rect{metrics.x, metrics.y, metrics.width, metrics.height});
}

struct ScrollViewPlan {
  Size viewport{};
  LayoutConstraints childConstraints{};
  std::vector<Size> childSizes{};
  layout::ScrollContentLayout contentLayout{};
};

template<typename MeasureChildFn>
ScrollViewPlan planScrollViewLayout(ScrollView const& scrollView, LayoutConstraints const& constraints,
                                    Size viewportHint, Point scrollOffset, MeasureChildFn&& measureChild) {
  ScrollViewPlan plan{};
  plan.viewport = scrollViewportHintForConstraints(scrollView.axis, constraints, viewportHint);

  if (plan.viewport.width <= 0.f || plan.viewport.height <= 0.f) {
    LayoutConstraints const premeasureConstraints =
        layout::scrollChildConstraints(scrollView.axis, constraints, plan.viewport);
    plan.childSizes.reserve(scrollView.children.size());
    for (Element const& child : scrollView.children) {
      plan.childSizes.push_back(measureChild(child, premeasureConstraints));
    }
    plan.viewport = viewportFromConstraints(
        scrollView.axis, layout::scrollContentSize(scrollView.axis, plan.childSizes), constraints);
  }

  plan.childConstraints = layout::scrollChildConstraints(scrollView.axis, constraints, plan.viewport);
  if (plan.childSizes.empty()) {
    plan.childSizes.reserve(scrollView.children.size());
    for (Element const& child : scrollView.children) {
      plan.childSizes.push_back(measureChild(child, plan.childConstraints));
    }
  }
  plan.contentLayout =
      layout::layoutScrollContent(scrollView.axis, plan.viewport, scrollOffset, plan.childSizes);
  return plan;
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
    MountContext childCtx = ctx.childWithSharedScope(fixedConstraints(layoutPlan.slots[index].assignedSize), {});
    auto childNode = children[index].mount(childCtx);
    if (childNode) {
      detail::setLayoutPosition(*childNode, layoutPlan.slots[index].origin);
      group->appendChild(std::move(childNode));
    }
  }
  return group;
}

Size ScrollView::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                         LayoutHints const&, TextSystem& textSystem) const {
  ContainerMeasureScope scope(ctx);
  ScrollViewPlan const plan =
      planScrollViewLayout(*this, constraints, Size{}, Point{},
                           [&](Element const& child, LayoutConstraints const& childConstraints) {
                             return measureChild(child, ctx, childConstraints, textSystem);
                           });
  return plan.viewport;
}

std::unique_ptr<scenegraph::SceneNode> ScrollView::mount(MountContext& ctx) const {
  ScrollViewPlan const plan =
      planScrollViewLayout(*this, ctx.constraints(), Size{}, *scrollOffset,
                           [&](Element const& child, LayoutConstraints const& childConstraints) {
                             return measureChild(child, ctx.measureContext(), childConstraints, ctx.textSystem());
                           });
  Size const viewport = plan.viewport;
  Size const content = plan.contentLayout.contentSize;
  viewportSize = viewport;
  contentSize = content;
  scrollOffset = plan.contentLayout.clampedOffset;

  auto viewportNode = std::make_unique<scenegraph::RectNode>(Rect{0.f, 0.f, viewport.width, viewport.height});
  viewportNode->setClipsContents(true);

  auto contentGroup = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, std::max(0.f, content.width), std::max(0.f, content.height)});

  layout::ScrollContentLayout initialLayout =
      layout::layoutScrollContent(axis, viewport, Point{}, plan.childSizes);
  for (std::size_t index = 0; index < children.size(); ++index) {
    MountContext childCtx = ctx.childWithSharedScope(fixedConstraints(initialLayout.slots[index].assignedSize), {});
    auto childNode = children[index].mount(childCtx);
    if (childNode) {
      detail::setLayoutPosition(*childNode, initialLayout.slots[index].origin);
      contentGroup->appendChild(std::move(childNode));
    }
  }

  auto* rawContentGroup = contentGroup.get();
  rawContentGroup->setPosition(Point{-plan.contentLayout.clampedOffset.x,
                                     -plan.contentLayout.clampedOffset.y});
  viewportNode->appendChild(std::move(contentGroup));
  auto* rawViewportNode = viewportNode.get();

  Point const scrollRange = layout::maxScrollOffset(axis, viewport, content);
  bool const showsVerticalIndicator = scrollRange.y > 0.f;
  bool const showsHorizontalIndicator = scrollRange.x > 0.f;
  bool const showsAnyIndicator = showsVerticalIndicator || showsHorizontalIndicator;
  scenegraph::RectNode* rawIndicatorOverlay = nullptr;
  scenegraph::RectNode* rawVerticalIndicator = nullptr;
  scenegraph::RectNode* rawHorizontalIndicator = nullptr;
  if (showsAnyIndicator) {
    Color const indicatorColor = scrollIndicatorColor(ctx.environmentBinding());
    auto indicatorOverlay =
        std::make_unique<scenegraph::RectNode>(Rect{0.f, 0.f, viewport.width, viewport.height});
    indicatorOverlay->setOpacity(0.f);

    layout::ScrollIndicatorMetrics const verticalIndicator =
        layout::makeVerticalIndicator(plan.contentLayout.clampedOffset, viewport, content,
                                      showsHorizontalIndicator);
    if (verticalIndicator.visible()) {
      auto indicator = std::make_unique<scenegraph::RectNode>(
          Rect{verticalIndicator.x, verticalIndicator.y, verticalIndicator.width,
               verticalIndicator.height},
          FillStyle::solid(indicatorColor),
          StrokeStyle::none(),
          CornerRadius{verticalIndicator.width * 0.5f});
      rawVerticalIndicator = indicator.get();
      indicatorOverlay->appendChild(std::move(indicator));
    }

    layout::ScrollIndicatorMetrics const horizontalIndicator =
        layout::makeHorizontalIndicator(plan.contentLayout.clampedOffset, viewport, content,
                                        showsVerticalIndicator);
    if (horizontalIndicator.visible()) {
      auto indicator = std::make_unique<scenegraph::RectNode>(
          Rect{horizontalIndicator.x, horizontalIndicator.y, horizontalIndicator.width,
               horizontalIndicator.height},
          FillStyle::solid(indicatorColor),
          StrokeStyle::none(),
          CornerRadius{horizontalIndicator.height * 0.5f});
      rawHorizontalIndicator = indicator.get();
      indicatorOverlay->appendChild(std::move(indicator));
    }

    rawIndicatorOverlay = indicatorOverlay.get();
    viewportNode->appendChild(std::move(indicatorOverlay));
  }

  Signal<Point> offsetState = scrollOffset;
  Signal<Size> viewportState = viewportSize;
  Signal<Size> contentState = contentSize;
  ScrollAxis const scrollAxis = axis;
  auto childSizes = std::make_shared<std::vector<Size>>(plan.childSizes);
  Reactive::SmallFn<void()> requestRedraw = ctx.redrawCallback();
  Reactive::withOwner(ctx.owner(), [rawContentGroup, rawVerticalIndicator, rawHorizontalIndicator,
                                    offsetState, viewportState, contentState,
                                    scrollAxis, childSizes,
                                    requestRedraw = std::move(requestRedraw)]() mutable {
    Reactive::Effect([rawContentGroup, rawVerticalIndicator, rawHorizontalIndicator,
                      offsetState, viewportState, contentState, scrollAxis,
                      childSizes, requestRedraw]() mutable {
      layout::ScrollContentLayout plan = layout::layoutScrollContent(
          scrollAxis, viewportState.get(), offsetState.get(), *childSizes);
      rawContentGroup->setPosition(Point{-plan.clampedOffset.x, -plan.clampedOffset.y});
      if (rawVerticalIndicator) {
        setIndicatorBounds(*rawVerticalIndicator,
                           layout::makeVerticalIndicator(plan.clampedOffset,
                                                         viewportState.get(),
                                                         contentState.get(),
                                                         rawHorizontalIndicator != nullptr));
      }
      if (rawHorizontalIndicator) {
        setIndicatorBounds(*rawHorizontalIndicator,
                           layout::makeHorizontalIndicator(plan.clampedOffset,
                                                           viewportState.get(),
                                                           contentState.get(),
                                                           rawVerticalIndicator != nullptr));
      }
      if (plan.clampedOffset != offsetState.peek()) {
        offsetState = plan.clampedOffset;
      }
      if (requestRedraw) {
        requestRedraw();
      }
    });
  });

  auto interaction = std::make_unique<scenegraph::InteractionData>();
  Reactive::SmallFn<void()> const scrollRedraw = ctx.redrawCallback();
  auto revealIndicators = [rawIndicatorOverlay, scrollRedraw] {
    if (rawIndicatorOverlay) {
      rawIndicatorOverlay->setOpacity(1.f);
      if (scrollRedraw) {
        scrollRedraw();
      }
    }
  };

  auto dragging = std::make_shared<bool>(false);
  auto downPoint = std::make_shared<Point>();
  bool const dragScroll = dragScrollEnabled;
  interaction->onPointerDown = [dragScroll, dragging, downPoint, offsetState](Point point) {
    if (!dragScroll) {
      return;
    }
    *dragging = true;
    *downPoint = Point{point.x + offsetState.peek().x, point.y + offsetState.peek().y};
  };
  interaction->onPointerUp = [dragging](Point) {
    *dragging = false;
  };
  interaction->onPointerMove =
      [dragScroll, dragging, downPoint, offsetState, viewportState, contentState,
       scrollAxis, revealIndicators](Point point) {
        if (!dragScroll || !*dragging) {
          return;
        }
        Point const next{downPoint->x - point.x, downPoint->y - point.y};
        offsetState = layout::clampScrollOffset(scrollAxis, next, viewportState.get(), contentState.get());
        revealIndicators();
      };
  interaction->onScroll = [offsetState, viewportState, contentState, scrollAxis,
                           revealIndicators](Vec2 delta) {
    Point next = offsetState.get();
    if (scrollAxis == ScrollAxis::Vertical || scrollAxis == ScrollAxis::Both) {
      next.y -= delta.y;
    }
    if (scrollAxis == ScrollAxis::Horizontal || scrollAxis == ScrollAxis::Both) {
      next.x -= delta.x;
    }
    offsetState = layout::clampScrollOffset(scrollAxis, next, viewportState.get(), contentState.get());
    revealIndicators();
  };
  viewportNode->setInteraction(std::move(interaction));
  ScrollView scrollView = *this;
  rawViewportNode->setLayoutConstraints(ctx.constraints());
  rawViewportNode->setRelayout([rawViewportNode, rawContentGroup, rawIndicatorOverlay,
                                rawVerticalIndicator, rawHorizontalIndicator,
                                scrollView = std::move(scrollView), offsetState,
                                viewportState, contentState, childSizes](
                                   LayoutConstraints const& constraints) mutable {
    auto mountedChildren = rawContentGroup->children();
    ScrollViewPlan plan{};
    plan.viewport = scrollViewportHintForConstraints(scrollView.axis, constraints, Size{});
    LayoutConstraints premeasureConstraints =
        layout::scrollChildConstraints(scrollView.axis, constraints, plan.viewport);
    childSizes->clear();
    childSizes->reserve(mountedChildren.size());
    for (std::unique_ptr<scenegraph::SceneNode>& child : mountedChildren) {
      if (child && child->relayout(premeasureConstraints)) {
        childSizes->push_back(child->size());
      } else {
        childSizes->push_back(child ? child->size() : Size{});
      }
    }
    if (plan.viewport.width <= 0.f || plan.viewport.height <= 0.f) {
      plan.viewport = viewportFromConstraints(
          scrollView.axis, layout::scrollContentSize(scrollView.axis, *childSizes), constraints);
    }
    plan.childConstraints = layout::scrollChildConstraints(scrollView.axis, constraints, plan.viewport);
    for (std::size_t i = 0; i < mountedChildren.size(); ++i) {
      if (mountedChildren[i] && mountedChildren[i]->relayout(plan.childConstraints)) {
        (*childSizes)[i] = mountedChildren[i]->size();
      }
    }
    plan.contentLayout =
        layout::layoutScrollContent(scrollView.axis, plan.viewport, offsetState.peek(), *childSizes);
    layout::ScrollContentLayout const childLayout =
        layout::layoutScrollContent(scrollView.axis, plan.viewport, Point{}, *childSizes);
    for (std::size_t i = 0; i < mountedChildren.size() && i < childLayout.slots.size(); ++i) {
      if (mountedChildren[i]) {
        mountedChildren[i]->setPosition(childLayout.slots[i].origin);
      }
    }
    rawViewportNode->setSize(plan.viewport);
    rawContentGroup->setSize(plan.contentLayout.contentSize);
    rawContentGroup->setPosition(Point{-plan.contentLayout.clampedOffset.x,
                                       -plan.contentLayout.clampedOffset.y});
    if (rawIndicatorOverlay) {
      rawIndicatorOverlay->setSize(plan.viewport);
    }
    bool const showsHorizontalIndicator = rawHorizontalIndicator != nullptr;
    bool const showsVerticalIndicator = rawVerticalIndicator != nullptr;
    if (rawVerticalIndicator) {
      setIndicatorBounds(*rawVerticalIndicator,
                         layout::makeVerticalIndicator(plan.contentLayout.clampedOffset,
                                                       plan.viewport,
                                                       plan.contentLayout.contentSize,
                                                       showsHorizontalIndicator));
    }
    if (rawHorizontalIndicator) {
      setIndicatorBounds(*rawHorizontalIndicator,
                         layout::makeHorizontalIndicator(plan.contentLayout.clampedOffset,
                                                         plan.viewport,
                                                         plan.contentLayout.contentSize,
                                                         showsVerticalIndicator));
    }
    viewportState = plan.viewport;
    contentState = plan.contentLayout.contentSize;
    offsetState = plan.contentLayout.clampedOffset;
  });

  return viewportNode;
}

} // namespace flux
