#include <Flux/Detail/Runtime.hpp>
#include <Flux/Reactive/Animation.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/ScrollView.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/Layout/Algorithms/ScrollLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"
#include "UI/SceneBuilder/NodeReuse.hpp"

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
                                            std::unique_ptr<SceneNode> existing) {
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

  std::unique_ptr<ModifierSceneNode> modifier = build::releaseAs<ModifierSceneNode>(std::move(existing));
  if (!modifier) {
    modifier = std::make_unique<ModifierSceneNode>(ctx.nodeId());
  }
  std::unique_ptr<SceneNode> existingViewportGroup{};
  if (!modifier->children().empty()) {
    std::vector<std::unique_ptr<SceneNode>> children = modifier->releaseChildren();
    if (!children.empty()) {
      existingViewportGroup = std::move(children.front());
    }
  }
  std::unique_ptr<SceneNode> viewportGroup = build::releasePlainGroup(std::move(existingViewportGroup));
  if (!viewportGroup) {
    viewportGroup = std::make_unique<SceneNode>(ctx.childId(LocalId::fromString("$content")));
  }
  ReusableSceneNodes reusableViewport = releaseReusableChildren(*viewportGroup);

  NodeId const scrolledGroupId = SceneTree::childId(viewportGroup->id(), LocalId::fromString("$scroll"));
  std::unique_ptr<SceneNode> existingScrolledGroup = takeReusableNode(reusableViewport, scrolledGroupId);
  std::unique_ptr<SceneNode> scrolledGroup = build::releasePlainGroup(std::move(existingScrolledGroup));
  if (!scrolledGroup) {
    scrolledGroup = std::make_unique<SceneNode>(scrolledGroupId);
  }
  ReusableSceneNodes reusable = releaseReusableChildren(*scrolledGroup);

  std::vector<std::unique_ptr<SceneNode>> scrolledChildren{};
  scrolledChildren.reserve(contentChildren.size());
  for (std::size_t i = 0; i < contentChildren.size(); ++i) {
    Element const& child = contentChildren[i];
    LocalId const local = build::childLocalId(child, i);
    NodeId const childId = SceneTree::childId(scrolledGroup->id(), local);
    std::unique_ptr<SceneNode> reuse = takeReusableNode(reusable, childId);
    layout::ScrollChildSlot const& slot = scrollLayout.slots[i];
    ctx.recordMeasuredSize(child, local, childConstraints, LayoutHints{}, plan.childSizes[i]);
    std::unique_ptr<SceneNode> childNode =
        ctx.buildChild(child, local, childConstraints, LayoutHints{},
                       Point{ctx.contentOrigin().x + slot.origin.x, ctx.contentOrigin().y + slot.origin.y},
                       slot.assignedSize, slot.assignedSize.width > 0.f, slot.assignedSize.height > 0.f,
                       std::move(reuse));
    childNode->position.x += slot.origin.x;
    childNode->position.y += slot.origin.y;
    scrolledChildren.push_back(std::move(childNode));
  }
  scrolledGroup->replaceChildren(std::move(scrolledChildren));
  build::setGroupBounds(*scrolledGroup, contentSize);

  auto updateIndicatorNode =
      [&](ReusableSceneNodes& reusableMap, NodeId indicatorId, layout::ScrollIndicatorMetrics const& metrics,
          bool vertical) -> std::unique_ptr<SceneNode> {
    std::unique_ptr<RectSceneNode> rectNode = takeReusableNodeAs<RectSceneNode>(reusableMap, indicatorId);
    if (!rectNode) {
      rectNode = std::make_unique<RectSceneNode>(indicatorId);
    }
    bool dirty = false;
    dirty |= build::updateIfChanged(rectNode->size, Size{metrics.width, metrics.height});
    dirty |= build::updateIfChanged(
        rectNode->cornerRadius, CornerRadius{vertical ? metrics.width * 0.5f : metrics.height * 0.5f});
    dirty |= build::updateIfChanged(rectNode->fill, FillStyle::solid(indicatorColor));
    dirty |= build::updateIfChanged(rectNode->stroke, StrokeStyle::none());
    dirty |= build::updateIfChanged(rectNode->shadow, ShadowStyle::none());
    if (dirty) {
      rectNode->invalidatePaint();
      rectNode->markBoundsDirty();
    }
    rectNode->position = Point{metrics.x, metrics.y};
    rectNode->recomputeBounds();
    return rectNode;
  };

  std::vector<std::unique_ptr<SceneNode>> viewportChildren{};
  viewportChildren.reserve(2);
  viewportChildren.push_back(std::move(scrolledGroup));
  if (showsAnyIndicator) {
    NodeId const indicatorOverlayId =
        SceneTree::childId(viewportGroup->id(), LocalId::fromString("$indicators"));
    std::unique_ptr<ModifierSceneNode> indicatorOverlay =
        takeReusableNodeAs<ModifierSceneNode>(reusableViewport, indicatorOverlayId);
    if (!indicatorOverlay) {
      indicatorOverlay = std::make_unique<ModifierSceneNode>(indicatorOverlayId);
    }
    indicatorOverlay->clip.reset();
    indicatorOverlay->opacity = indicatorOpacity;
    indicatorOverlay->blendMode = BlendMode::Normal;
    indicatorOverlay->fill = FillStyle::none();
    indicatorOverlay->stroke = StrokeStyle::none();
    indicatorOverlay->shadow = ShadowStyle::none();
    indicatorOverlay->cornerRadius = {};
    indicatorOverlay->position = {};

    std::vector<std::unique_ptr<SceneNode>> indicatorChildren{};
    indicatorChildren.reserve(2);
    if (verticalIndicator.visible()) {
      indicatorChildren.push_back(updateIndicatorNode(
          reusableViewport, SceneTree::childId(indicatorOverlay->id(), LocalId::fromString("$v-indicator")),
          verticalIndicator, true));
    }
    if (horizontalIndicator.visible()) {
      indicatorChildren.push_back(updateIndicatorNode(
          reusableViewport, SceneTree::childId(indicatorOverlay->id(), LocalId::fromString("$h-indicator")),
          horizontalIndicator, false));
    }
    indicatorOverlay->replaceChildren(std::move(indicatorChildren));
    indicatorOverlay->recomputeBounds();
    viewportChildren.push_back(std::move(indicatorOverlay));
  }
  viewportGroup->replaceChildren(std::move(viewportChildren));
  build::setGroupBounds(*viewportGroup, Size{std::max(contentSize.width, viewport.width),
                                             std::max(contentSize.height, viewport.height)});

  auto interaction = ctx.makeInteractionData();
  if (!interaction) {
    interaction = std::make_unique<InteractionData>();
  }
  interaction->stableTargetKey = ctx.key();
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

  modifier->replaceChildren({});
  modifier->appendChild(std::move(viewportGroup));
  modifier->clip = Rect{0.f, 0.f, viewport.width, viewport.height};
  modifier->opacity = 1.f;
  modifier->fill = FillStyle::none();
  modifier->stroke = StrokeStyle::none();
  modifier->shadow = ShadowStyle::none();
  modifier->cornerRadius = {};
  modifier->recomputeBounds();

  ComponentBuildResult result{};
  result.node = std::move(modifier);
  result.geometrySize = viewport;
  result.hasGeometrySize = true;
  result.interaction = std::move(interaction);
  return result;
}

} // namespace detail

} // namespace flux
