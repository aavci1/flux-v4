#include <Flux/Detail/Runtime.hpp>
#include <Flux/Reactive/Animation.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/ScrollView.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/Layout/Algorithms/ScrollLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"

#include <cmath>
#include <optional>
#include <typeindex>
#include <utility>
#include <vector>

namespace flux {

namespace build = detail::build;

Point clampScrollOffset(ScrollAxis axis, Point offset, Size const& viewport, Size const& content) {
  return layout::clampScrollOffset(axis, offset, viewport, content);
}

namespace {

struct ScrollViewPlan {
  Size viewport{};
  LayoutConstraints childConstraints{};
  std::vector<Size> childSizes{};
  layout::ScrollContentLayout contentLayout{};
};

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

Size scrollViewportHintForMeasure(LayoutConstraints const& constraints, LayoutHints const& hints) {
  Size viewportHint{};
  if (build::zStackAxisStretches(hints.zStackHorizontalAlign) && std::isfinite(constraints.maxWidth) &&
      constraints.maxWidth > 0.f) {
    viewportHint.width = constraints.maxWidth;
  }
  if (build::zStackAxisStretches(hints.zStackVerticalAlign) && std::isfinite(constraints.maxHeight) &&
      constraints.maxHeight > 0.f) {
    viewportHint.height = constraints.maxHeight;
  }
  return viewportHint;
}

std::unique_ptr<scenegraph::SceneNode>
takeExistingChild(std::vector<std::unique_ptr<scenegraph::SceneNode>>& existingChildren,
                  std::size_t index) {
  if (index >= existingChildren.size()) {
    return nullptr;
  }
  return std::move(existingChildren[index]);
}

template<typename MeasureChildFn>
ScrollViewPlan planScrollViewLayout(ScrollView const& scrollView, LayoutConstraints const& constraints,
                                    Size viewportHint, Point scrollOffset, MeasureChildFn&& measureChild) {
  ScrollViewPlan plan{};
  plan.viewport = scrollViewportHintForConstraints(scrollView.axis, constraints, viewportHint);

  if (plan.viewport.width <= 0.f || plan.viewport.height <= 0.f) {
    LayoutConstraints const premeasureChildConstraints =
        layout::scrollChildConstraints(scrollView.axis, constraints, plan.viewport);
    plan.childSizes.reserve(scrollView.children.size());
    for (std::size_t i = 0; i < scrollView.children.size(); ++i) {
      Element const& child = scrollView.children[i];
      LocalId const local = build::childLocalId(child, i);
      plan.childSizes.push_back(measureChild(child, local, premeasureChildConstraints));
    }
    plan.viewport = layout::resolveMeasuredScrollViewSize(
        scrollView.axis, layout::scrollContentSize(scrollView.axis, plan.childSizes), constraints);
  }

  plan.childConstraints = layout::scrollChildConstraints(scrollView.axis, constraints, plan.viewport);
  if (plan.childSizes.empty()) {
    plan.childSizes.reserve(scrollView.children.size());
    for (std::size_t i = 0; i < scrollView.children.size(); ++i) {
      Element const& child = scrollView.children[i];
      LocalId const local = build::childLocalId(child, i);
      plan.childSizes.push_back(measureChild(child, local, plan.childConstraints));
    }
  }
  plan.contentLayout =
      layout::layoutScrollContent(scrollView.axis, plan.viewport, scrollOffset, plan.childSizes);
  return plan;
}

} // namespace

Size ScrollView::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                         TextSystem& textSystem) const {
  ContainerMeasureScope scope(ctx);
  ScrollViewPlan const plan =
      planScrollViewLayout(*this, constraints, scrollViewportHintForMeasure(constraints, hints), Point{},
                           [&](Element const& child, LocalId, LayoutConstraints const& childConstraints) {
                             return child.measure(ctx, childConstraints, LayoutHints{}, textSystem);
                           });
  return plan.viewport;
}

namespace detail {

ComponentBuildResult buildMeasuredComponent(ScrollView const& scrollView, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  ComponentKey scrollStateKey = ctx.key();
  scrollStateKey.push_back(LocalId::fromString("$scroll-state"));
  StateStore* const store = StateStore::current();
  if (store) {
    store->pushComponent(scrollStateKey, std::type_index(typeid(ScrollView)));
  }
  struct ScrollComponentPop {
    StateStore* store = nullptr;
    ~ScrollComponentPop() {
      if (store) {
        store->popComponent();
      }
    }
  } scrollComponentPop{store};

  State<Point> offsetState = scrollView.scrollOffset;
  if (!offsetState.signal && store) {
    offsetState = State<Point>{&store->claimSlot<Signal<Point>>(Point{})};
  }
  State<Size> viewportState = scrollView.viewportSize;
  if (!viewportState.signal && store) {
    viewportState = State<Size>{&store->claimSlot<Signal<Size>>(Size{})};
  }
  State<Size> contentState = scrollView.contentSize;
  if (!contentState.signal && store) {
    contentState = State<Size>{&store->claimSlot<Signal<Size>>(Size{})};
  }
  Animation<float>* indicatorOpacityAnimation = nullptr;
  if (store) {
    indicatorOpacityAnimation = &store->claimSlot<Animation<float>>(0.f);
  }
  State<Point> downPointState{};
  State<bool> draggingState{};
  if (store) {
    downPointState = State<Point>{&store->claimSlot<Signal<Point>>(Point{})};
    draggingState = State<bool>{&store->claimSlot<Signal<bool>>(false)};
  }

  ScrollAxis const axis = scrollView.axis;
  Point scrollOffset = offsetState.signal ? *offsetState : Point{};
  std::vector<Element> const& contentChildren = scrollView.children;
  Size viewportHint{};
  if (ctx.hasAssignedWidth()) {
    viewportHint.width = std::max(0.f, ctx.contentAssignedSize().width);
  }
  if (ctx.hasAssignedHeight()) {
    viewportHint.height = std::max(0.f, ctx.contentAssignedSize().height);
  }
  if (detail::ElementModifiers const* mods = ctx.modifiers()) {
    float const horizontalPadding =
        std::max(0.f, mods->padding.left) + std::max(0.f, mods->padding.right);
    float const verticalPadding =
        std::max(0.f, mods->padding.top) + std::max(0.f, mods->padding.bottom);
    if (mods->sizeWidth > 0.f) {
      viewportHint.width = std::max(0.f, mods->sizeWidth - horizontalPadding);
    }
    if (mods->sizeHeight > 0.f) {
      viewportHint.height = std::max(0.f, mods->sizeHeight - verticalPadding);
    }
  }
  ScrollViewPlan const plan =
      planScrollViewLayout(scrollView, ctx.innerConstraints(), viewportHint, scrollOffset,
                           [&](Element const& child, LocalId local, LayoutConstraints const& childConstraints) {
                             return ctx.measureChild(child, local, childConstraints, LayoutHints{});
                           });
  Size const viewport = plan.viewport;
  if (viewportState.signal && !build::sizeApproximatelyEqual(*viewportState, viewport)) {
    viewportState.setSilently(viewport);
  }
  LayoutConstraints const& childConstraints = plan.childConstraints;
  layout::ScrollContentLayout const& scrollLayout = plan.contentLayout;
  Size const contentSize = scrollLayout.contentSize;
  if (contentState.signal && !build::sizeApproximatelyEqual(*contentState, contentSize)) {
    contentState.setSilently(contentSize);
  }
  scrollOffset = scrollLayout.clampedOffset;
  if (offsetState.signal && scrollOffset != *offsetState) {
    offsetState.setSilently(scrollOffset);
  }

  Point const scrollRange = layout::maxScrollOffset(axis, viewport, contentSize);
  bool const showsVerticalIndicator = scrollRange.y > 0.f;
  bool const showsHorizontalIndicator = scrollRange.x > 0.f;
  bool const showsAnyIndicator = showsVerticalIndicator || showsHorizontalIndicator;
  Theme const& theme = ctx.theme();
  Color const indicatorColor = build::scrollIndicatorColorForTheme(theme);
  Transition const indicatorShow = Transition::instant();
  Transition const indicatorHide = Transition::linear(theme.durationMedium).delayed(0.85f);
  float const indicatorOpacity = indicatorOpacityAnimation ? indicatorOpacityAnimation->get() : 0.f;
  layout::ScrollIndicatorMetrics const verticalIndicator =
      layout::makeVerticalIndicator(scrollOffset, viewport, contentSize, showsHorizontalIndicator);
  layout::ScrollIndicatorMetrics const horizontalIndicator =
      layout::makeHorizontalIndicator(scrollOffset, viewport, contentSize, showsVerticalIndicator);

  std::vector<std::unique_ptr<scenegraph::SceneNode>> existingViewportChildren{};
  std::unique_ptr<scenegraph::RectNode> viewportNode{};
  if (existing && existing->kind() == scenegraph::SceneNodeKind::Rect) {
    ctx.recordNodeReuse();
    viewportNode = std::unique_ptr<scenegraph::RectNode>(
        static_cast<scenegraph::RectNode*>(existing.release()));
    existingViewportChildren = viewportNode->releaseChildren();
  } else {
    viewportNode =
        std::make_unique<scenegraph::RectNode>(Rect {0.f, 0.f, viewport.width, viewport.height});
  }
  viewportNode->setBounds(Rect {0.f, 0.f, viewport.width, viewport.height});
  viewportNode->setClipsContents(true);

  std::vector<std::unique_ptr<scenegraph::SceneNode>> existingScrolledChildren{};
  std::unique_ptr<scenegraph::GroupNode> scrolledGroup{};
  if (!existingViewportChildren.empty() &&
      existingViewportChildren.front() &&
      existingViewportChildren.front()->kind() == scenegraph::SceneNodeKind::Group) {
    ctx.recordNodeReuse();
    scrolledGroup = std::unique_ptr<scenegraph::GroupNode>(
        static_cast<scenegraph::GroupNode*>(existingViewportChildren.front().release()));
    existingScrolledChildren = scrolledGroup->releaseChildren();
  } else {
    scrolledGroup = std::make_unique<scenegraph::GroupNode>();
  }

  std::vector<std::unique_ptr<scenegraph::SceneNode>> scrolledChildren{};
  scrolledChildren.reserve(contentChildren.size());
  for (std::size_t i = 0; i < contentChildren.size(); ++i) {
    Element const& child = contentChildren[i];
    LocalId const local = build::childLocalId(child, i);
    layout::ScrollChildSlot const& slot = scrollLayout.slots[i];
    ctx.recordMeasuredSize(child, local, childConstraints, LayoutHints{}, plan.childSizes[i]);
    std::unique_ptr<scenegraph::SceneNode> childNode =
        ctx.buildChild(child, local, childConstraints, LayoutHints{},
                       Point{ctx.contentOrigin().x + slot.origin.x, ctx.contentOrigin().y + slot.origin.y},
                       slot.assignedSize, slot.assignedSize.width > 0.f,
                       slot.assignedSize.height > 0.f,
                       takeExistingChild(existingScrolledChildren, i));
    Point childOrigin = slot.origin;
    if (axis == ScrollAxis::Vertical || axis == ScrollAxis::Both) {
      childOrigin.y += scrollOffset.y;
    }
    if (axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both) {
      childOrigin.x += scrollOffset.x;
    }
    childNode->setPosition(Point {
        childNode->position().x + childOrigin.x,
        childNode->position().y + childOrigin.y,
    });
    scrolledChildren.push_back(std::move(childNode));
  }
  scrolledGroup->replaceChildren(std::move(scrolledChildren));
  build::setAssignedGroupBounds(*scrolledGroup, contentSize);
  scrolledGroup->setPosition(Point {-scrollOffset.x, -scrollOffset.y});
  viewportNode->appendChild(std::move(scrolledGroup));
  if (showsAnyIndicator) {
    auto indicatorOverlay =
        std::make_unique<scenegraph::RectNode>(Rect {0.f, 0.f, viewport.width, viewport.height});
    indicatorOverlay->setOpacity(indicatorOpacity);
    if (verticalIndicator.visible()) {
      indicatorOverlay->appendChild(std::make_unique<scenegraph::RectNode>(
          Rect {verticalIndicator.x, verticalIndicator.y, verticalIndicator.width, verticalIndicator.height},
          FillStyle::solid(indicatorColor),
          StrokeStyle::none(),
          CornerRadius {verticalIndicator.width * 0.5f}
      ));
    }
    if (horizontalIndicator.visible()) {
      indicatorOverlay->appendChild(std::make_unique<scenegraph::RectNode>(
          Rect {horizontalIndicator.x, horizontalIndicator.y, horizontalIndicator.width, horizontalIndicator.height},
          FillStyle::solid(indicatorColor),
          StrokeStyle::none(),
          CornerRadius {horizontalIndicator.height * 0.5f}
      ));
    }
    viewportNode->appendChild(std::move(indicatorOverlay));
  }

  auto interaction = ctx.makeInteractionData();
  if (!interaction) {
    interaction = std::make_unique<scenegraph::InteractionData>();
  }
  interaction->stableTargetKey = ctx.interactionKey();
  std::function<void(Point)> priorPointerDown = interaction->onPointerDown;
  std::function<void(Point)> priorPointerMove = interaction->onPointerMove;
  std::function<void(Point)> priorPointerUp = interaction->onPointerUp;
  std::function<void(Vec2)> priorScroll = interaction->onScroll;

  bool const dragScroll = scrollView.dragScrollEnabled;
  auto revealIndicators = [indicatorOpacityAnimation, indicatorShow, indicatorHide, showsAnyIndicator]() {
    if (!indicatorOpacityAnimation || !showsAnyIndicator) {
      return;
    }
    indicatorOpacityAnimation->set(1.f, indicatorShow);
    indicatorOpacityAnimation->set(0.f, indicatorHide);
  };
  interaction->onPointerDown =
      [priorPointerDown, dragScroll, draggingState, downPointState, offsetState](Point point) {
        if (priorPointerDown) {
          priorPointerDown(point);
        }
        if (!dragScroll || !draggingState.signal || !downPointState.signal || !offsetState.signal) {
          return;
        }
        draggingState = true;
        downPointState = Point{point.x + (*offsetState).x, point.y + (*offsetState).y};
      };
  interaction->onPointerUp = [priorPointerUp, dragScroll, draggingState](Point point) {
    if (priorPointerUp) {
      priorPointerUp(point);
    }
    if (!dragScroll || !draggingState.signal) {
      return;
    }
    draggingState = false;
  };
  interaction->onPointerMove =
      [priorPointerMove, dragScroll, draggingState, downPointState, axis, contentState, viewport, offsetState,
       revealIndicators](Point point) {
        if (priorPointerMove) {
          priorPointerMove(point);
        }
        if (!dragScroll || !draggingState.signal || !downPointState.signal || !offsetState.signal ||
            !contentState.signal || !*draggingState) {
          return;
        }
        Point const next{(*downPointState).x - point.x, (*downPointState).y - point.y};
        offsetState = layout::clampScrollOffset(axis, next, viewport, *contentState);
        revealIndicators();
      };
  interaction->onScroll =
      [priorScroll, axis, offsetState, contentState, viewport, revealIndicators](Vec2 delta) {
        if (priorScroll) {
          priorScroll(delta);
        }
        if (!offsetState.signal || !contentState.signal) {
          return;
        }
        Point next = *offsetState;
        if (axis == ScrollAxis::Vertical || axis == ScrollAxis::Both) {
          next.y -= delta.y;
        }
        if (axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both) {
          next.x -= delta.x;
        }
        offsetState = layout::clampScrollOffset(axis, next, viewport, *contentState);
        revealIndicators();
      };

  viewportNode->setInteraction(std::move(interaction));

  ComponentBuildResult result{};
  result.node = std::move(viewportNode);
  result.geometrySize = viewport;
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
