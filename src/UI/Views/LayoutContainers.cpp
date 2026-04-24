#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/Grid.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/OffsetView.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/Layout/Algorithms/GridLayout.hpp"
#include "UI/Layout/Algorithms/ScrollLayout.hpp"
#include "UI/Layout/Algorithms/StackLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"
#include "SceneGraph/SceneBounds.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace flux {
using namespace flux::layout;
namespace build = detail::build;

namespace {

std::unique_ptr<scenegraph::GroupNode>
reuseGroupNode(detail::ComponentBuildContext const& ctx, std::unique_ptr<scenegraph::SceneNode> existing,
               std::vector<std::unique_ptr<scenegraph::SceneNode>>& existingChildren) {
  existingChildren.clear();
  if (existing && existing->kind() == scenegraph::SceneNodeKind::Group) {
    ctx.recordNodeReuse();
    auto group = std::unique_ptr<scenegraph::GroupNode>(
        static_cast<scenegraph::GroupNode*>(existing.release()));
    existingChildren = group->releaseChildren();
    return group;
  }
  return std::make_unique<scenegraph::GroupNode>();
}

std::unique_ptr<scenegraph::SceneNode>
takeExistingChild(std::vector<std::unique_ptr<scenegraph::SceneNode>>& existingChildren,
                  std::size_t index) {
  if (index >= existingChildren.size()) {
    return nullptr;
  }
  return std::move(existingChildren[index]);
}

struct VStackPlan {
  LayoutConstraints childConstraints{};
  LayoutHints childHints{};
  StackLayoutResult layout{};
  std::vector<Size> measuredChildSizes{};
};

template<typename MeasureChildFn>
VStackPlan planVStackLayout(VStack const& stack, LayoutConstraints const& constraints, LayoutHints const&,
                            float assignedWidth, bool heightConstrained, float assignedHeight,
                            std::optional<StackLayoutResult> cachedLayout, MeasureChildFn&& measureChild) {
  VStackPlan plan{};
  float const availableWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  plan.childConstraints = constraints;
  plan.childConstraints.maxHeight = std::numeric_limits<float>::infinity();
  plan.childConstraints.maxWidth =
      availableWidth > 0.f ? availableWidth : std::numeric_limits<float>::infinity();
  clampLayoutMinToMax(plan.childConstraints);
  plan.childHints.vStackCrossAlign = stack.alignment;

  if (cachedLayout.has_value()) {
    plan.layout = *cachedLayout;
    return plan;
  }

  std::vector<Size> sizes{};
  sizes.reserve(stack.children.size());
  std::vector<StackMainAxisChild> stackChildren{};
  stackChildren.reserve(stack.children.size());
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LocalId const local = build::childLocalId(child, i);
    Size const size = measureChild(child, local, plan.childConstraints, plan.childHints);
    sizes.push_back(size);
    stackChildren.push_back(StackMainAxisChild{
        .naturalMainSize = size.height,
        .flexBasis = child.flexBasis(),
        .minMainSize = child.minMainSize(),
        .flexGrow = child.flexGrow(),
        .flexShrink = child.flexShrink(),
    });
  }

  if (!heightConstrained && !stack.children.empty()) {
    warnFlexGrowIfParentMainAxisUnconstrained(stack.children, heightConstrained);
  }

  StackMainAxisLayout const mainLayout =
      layoutStackMainAxis(stackChildren, stack.spacing, assignedHeight, heightConstrained,
                          stack.justifyContent);
  plan.layout = layoutStack(StackAxis::Vertical, stack.alignment, sizes, mainLayout.mainSizes,
                            mainLayout.itemSpacing, mainLayout.containerMainSize,
                            mainLayout.startOffset, assignedWidth, assignedWidth > 0.f);
  plan.measuredChildSizes = std::move(sizes);
  return plan;
}

struct HStackPlan {
  LayoutConstraints childConstraints{};
  LayoutHints rowHints{};
  StackLayoutResult layout{};
  std::vector<Size> measuredChildSizes{};
};

template<typename MeasureInitialFn, typename MeasureRowFn, typename ResetTraversalFn>
HStackPlan planHStackLayout(HStack const& stack, LayoutConstraints const& constraints, bool widthAssigned,
                            float assignedWidth, bool heightConstrained, float assignedHeight,
                            std::optional<StackLayoutResult> cachedLayout, MeasureInitialFn&& measureInitial,
                            MeasureRowFn&& measureRow, ResetTraversalFn&& resetTraversal) {
  HStackPlan plan{};
  plan.rowHints.hStackCrossAlign = stack.alignment;
  bool const stretchCrossAxis = stack.alignment == Alignment::Stretch && heightConstrained;

  plan.childConstraints = constraints;
  plan.childConstraints.maxWidth = std::numeric_limits<float>::infinity();
  plan.childConstraints.maxHeight =
      stretchCrossAxis ? assignedHeight : std::numeric_limits<float>::infinity();
  if (stack.children.size() == 1 && widthAssigned) {
    plan.childConstraints.maxWidth = assignedWidth;
  }
  clampLayoutMinToMax(plan.childConstraints);

  if (cachedLayout.has_value()) {
    plan.layout = *cachedLayout;
    return plan;
  }

  std::vector<Size> sizes{};
  sizes.reserve(stack.children.size());
  std::vector<StackMainAxisChild> stackChildren{};
  stackChildren.reserve(stack.children.size());
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LocalId const local = build::childLocalId(child, i);
    Size const size = measureInitial(child, local, plan.childConstraints);
    sizes.push_back(size);
    stackChildren.push_back(StackMainAxisChild{
        .naturalMainSize = size.width,
        .flexBasis = child.flexBasis(),
        .minMainSize = child.minMainSize(),
        .flexGrow = child.flexGrow(),
        .flexShrink = child.flexShrink(),
    });
  }

  if (!widthAssigned && !stack.children.empty()) {
    warnFlexGrowIfParentMainAxisUnconstrained(stack.children, widthAssigned);
  }
  StackMainAxisLayout const mainLayout =
      layoutStackMainAxis(stackChildren, stack.spacing, assignedWidth, widthAssigned,
                          stack.justifyContent);

  resetTraversal();

  float rowInnerHeight = 0.f;
  std::vector<Size> rowSizes{};
  rowSizes.reserve(stack.children.size());
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LocalId const local = build::childLocalId(child, i);
    LayoutConstraints childMeasure = constraints;
    childMeasure.maxWidth = mainLayout.mainSizes[i];
    childMeasure.maxHeight =
        stretchCrossAxis ? assignedHeight : std::numeric_limits<float>::infinity();
    clampLayoutMinToMax(childMeasure);
    Size const measured = measureRow(child, local, childMeasure, plan.rowHints);
    rowSizes.push_back(measured);
    rowInnerHeight = std::max(rowInnerHeight, measured.height);
  }

  resetTraversal();

  float const rowCrossSize = heightConstrained ? assignedHeight : rowInnerHeight;
  plan.layout = layoutStack(StackAxis::Horizontal, stack.alignment, rowSizes, mainLayout.mainSizes,
                            mainLayout.itemSpacing, mainLayout.containerMainSize,
                            mainLayout.startOffset, rowCrossSize, heightConstrained);
  plan.measuredChildSizes = std::move(rowSizes);
  return plan;
}

struct ZStackPlan {
  LayoutConstraints childConstraints{};
  LayoutHints childHints{};
  Size containerSize{};
  std::vector<Size> measuredChildSizes{};
};

template<typename MeasureChildFn>
ZStackPlan planZStackLayout(ZStack const& stack, LayoutConstraints const& constraints, Size initialSize,
                            std::optional<Size> cachedSize, MeasureChildFn&& measureChild) {
  ZStackPlan plan{};
  float innerWidth = initialSize.width;
  float innerHeight = initialSize.height;

  plan.childConstraints = constraints;
  plan.childConstraints.maxWidth =
      innerWidth > 0.f ? innerWidth : std::numeric_limits<float>::infinity();
  plan.childConstraints.maxHeight =
      innerHeight > 0.f ? innerHeight : std::numeric_limits<float>::infinity();
  clampLayoutMinToMax(plan.childConstraints);
  plan.childHints.zStackHorizontalAlign = stack.horizontalAlignment;
  plan.childHints.zStackVerticalAlign = stack.verticalAlignment;

  if (cachedSize.has_value()) {
    plan.containerSize = *cachedSize;
    return plan;
  }

  float maxWidth = 0.f;
  float maxHeight = 0.f;
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LocalId const local = build::childLocalId(child, i);
    Size const size = measureChild(child, local, plan.childConstraints, plan.childHints);
    plan.measuredChildSizes.push_back(size);
    maxWidth = std::max(maxWidth, size.width);
    maxHeight = std::max(maxHeight, size.height);
  }

  if (innerWidth <= 0.f) {
    innerWidth = maxWidth;
  }
  if (innerHeight <= 0.f) {
    innerHeight = maxHeight;
  }
  innerWidth = std::max(innerWidth, maxWidth);
  innerHeight = std::max(innerHeight, maxHeight);
  if (initialSize.width > 0.f) {
    innerWidth = std::min(innerWidth, initialSize.width);
  }
  if (initialSize.height > 0.f) {
    innerHeight = std::min(innerHeight, initialSize.height);
  }
  plan.containerSize = Size{innerWidth, innerHeight};
  return plan;
}

struct GridPlan {
  GridTrackMetrics metrics{};
  GridLayoutResult layout{};
  std::vector<Size> measuredChildSizes{};
};

template<typename MeasureChildFn>
GridPlan planGridLayout(Grid const& grid, LayoutConstraints const& constraints, float innerWidth,
                        float innerHeight, std::optional<GridLayoutResult> cachedLayout,
                        MeasureChildFn&& measureChild) {
  GridPlan plan{};
  std::size_t const childCount = grid.children.size();
  std::vector<std::size_t> spans(childCount, 1u);
  for (std::size_t i = 0; i < childCount && i < grid.columnSpans.size(); ++i) {
    spans[i] = grid.columnSpans[i];
  }
  plan.metrics = resolveGridTrackMetrics(grid.columns, spans, grid.horizontalSpacing,
                                         grid.verticalSpacing, innerWidth, innerWidth > 0.f,
                                         innerHeight, innerHeight > 0.f);

  if (cachedLayout.has_value()) {
    plan.layout = *cachedLayout;
    return plan;
  }

  std::vector<Size> sizes{};
  sizes.reserve(childCount);
  for (std::size_t i = 0; i < childCount; ++i) {
    Element const& child = grid.children[i];
    LocalId const local = build::childLocalId(child, i);
    LayoutConstraints const childConstraints = gridChildConstraints(constraints, plan.metrics, i);
    sizes.push_back(measureChild(child, local, childConstraints));
  }
  plan.layout = layoutGrid(plan.metrics, grid.horizontalSpacing, grid.verticalSpacing, innerWidth,
                           innerWidth > 0.f, innerHeight, innerHeight > 0.f, sizes);
  plan.measuredChildSizes = std::move(sizes);
  return plan;
}

struct OffsetPlan {
  LayoutConstraints childConstraints{};
  std::vector<Size> measuredChildSizes{};
  ScrollContentLayout layout{};
};

template<typename MeasureChildFn>
OffsetPlan planOffsetLayout(OffsetView const& offsetView, LayoutConstraints const& constraints, Size viewport,
                            MeasureChildFn&& measureChild) {
  OffsetPlan plan{};
  plan.childConstraints = layout::scrollChildConstraints(offsetView.axis, constraints, viewport);
  plan.measuredChildSizes.reserve(offsetView.children.size());
  for (std::size_t i = 0; i < offsetView.children.size(); ++i) {
    Element const& child = offsetView.children[i];
    LocalId const local = build::childLocalId(child, i);
    plan.measuredChildSizes.push_back(measureChild(child, local, plan.childConstraints));
  }
  plan.layout =
      layout::layoutScrollContent(offsetView.axis, viewport, offsetView.offset, plan.measuredChildSizes);
  return plan;
}

} // namespace

Size VStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                     TextSystem& textSystem) const {
  ComponentKey const layoutKey = ctx.currentElementKey();
  Element const* const currentElement = ctx.currentElement();
  ContainerMeasureScope scope(ctx);
  float const availableWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const availableHeight = stackMainAxisSpan(0.f, constraints.maxHeight);
  bool const widthAssigned = build::zStackAxisStretches(hints.zStackHorizontalAlign);
  bool const heightAssigned = build::zStackAxisStretches(hints.zStackVerticalAlign);
  float const assignedWidth = widthAssigned ? availableWidth : 0.f;
  float const assignedHeight = heightAssigned ? availableHeight : 0.f;
  bool const heightConstrained = heightAssigned && std::isfinite(assignedHeight) && assignedHeight > 0.f;

  VStackPlan const plan =
      planVStackLayout(*this, constraints, hints, assignedWidth, heightConstrained, assignedHeight,
                       std::nullopt,
                       [&](Element const& child, LocalId, LayoutConstraints const& childConstraints,
                           LayoutHints const& childHints) {
                         return child.measure(ctx, childConstraints, childHints, textSystem);
                       });
  StackLayoutResult const& layoutResult = plan.layout;
  if (currentElement && ctx.layoutCache()) {
    ctx.layoutCache()->recordStackLayout(
        detail::MeasureLayoutKey{
            .measureId = currentElement->measureId(),
            .componentKey = layoutKey,
            .constraints = constraints,
            .hints = hints,
        },
        layoutResult);
  }
  return layoutResult.containerSize;
}

Size HStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                     TextSystem& textSystem) const {
  ComponentKey const layoutKey = ctx.currentElementKey();
  Element const* const currentElement = ctx.currentElement();
  ContainerMeasureScope scope(ctx);
  float const availableHeight = stackMainAxisSpan(0.f, constraints.maxHeight);
  bool const heightAssigned = build::zStackAxisStretches(hints.zStackVerticalAlign);
  float const assignedHeight = heightAssigned ? availableHeight : 0.f;
  bool const heightConstrained = heightAssigned && std::isfinite(assignedHeight) && assignedHeight > 0.f;
  std::size_t const childCount = children.size();
  if (childCount == 0) {
    return {0.f, 0.f};
  }

  float const availableWidth = stackMainAxisSpan(0.f, constraints.maxWidth);
  bool const widthAssigned = build::zStackAxisStretches(hints.zStackHorizontalAlign);
  float const assignedWidth = widthAssigned ? availableWidth : 0.f;
  bool const widthConstrained = widthAssigned && std::isfinite(assignedWidth) && assignedWidth > 0.f;
  HStackPlan const plan =
      planHStackLayout(*this, constraints, widthConstrained, assignedWidth, heightConstrained,
                       assignedHeight, std::nullopt,
                       [&](Element const& child, LocalId, LayoutConstraints const& childConstraints) {
                         return child.measure(ctx, childConstraints, LayoutHints{}, textSystem);
                       },
                       [&](Element const& child, LocalId, LayoutConstraints const& childConstraints,
                           LayoutHints const& rowHints) {
                         return child.measure(ctx, childConstraints, rowHints, textSystem);
                       },
                       [&]() {
                         if (StateStore* store = StateStore::current()) {
                           store->resetSlotCursors();
                         }
                         ctx.rewindChildKeyIndex();
                       });
  StackLayoutResult const& layoutResult = plan.layout;
  if (currentElement && ctx.layoutCache()) {
    ctx.layoutCache()->recordStackLayout(
        detail::MeasureLayoutKey{
            .measureId = currentElement->measureId(),
            .componentKey = layoutKey,
            .constraints = constraints,
            .hints = hints,
        },
        layoutResult);
  }
  return layoutResult.containerSize;
}

Size ZStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                     TextSystem& textSystem) const {
  ComponentKey const layoutKey = ctx.currentElementKey();
  Element const* const currentElement = ctx.currentElement();
  ContainerMeasureScope scope(ctx);
  float const assignedWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedHeight = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerWidth = std::max(0.f, assignedWidth);
  float innerHeight = std::max(0.f, assignedHeight);

  ZStackPlan const plan =
      planZStackLayout(*this, constraints, Size{innerWidth, innerHeight}, std::nullopt,
                       [&](Element const& child, LocalId, LayoutConstraints const& childConstraints,
                           LayoutHints const& childHints) {
                         return child.measure(ctx, childConstraints, childHints, textSystem);
                       });
  if (currentElement && ctx.layoutCache()) {
    ctx.layoutCache()->recordZStackSize(
        detail::MeasureLayoutKey{
            .measureId = currentElement->measureId(),
            .componentKey = layoutKey,
            .constraints = constraints,
            .hints = hints,
        },
        plan.containerSize);
  }
  return plan.containerSize;
}

Size Grid::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                   TextSystem& textSystem) const {
  ComponentKey const layoutKey = ctx.currentElementKey();
  Element const* const currentElement = ctx.currentElement();
  ContainerMeasureScope scope(ctx);
  float const assignedWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedHeight = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  GridPlan const plan =
      planGridLayout(*this, constraints, assignedWidth, assignedHeight, std::nullopt,
                     [&](Element const& child, LocalId, LayoutConstraints const& childConstraints) {
                       return child.measure(ctx, childConstraints, LayoutHints{}, textSystem);
                     });
  if (currentElement && ctx.layoutCache()) {
    ctx.layoutCache()->recordGridLayout(
        detail::MeasureLayoutKey{
            .measureId = currentElement->measureId(),
            .componentKey = layoutKey,
            .constraints = constraints,
            .hints = hints,
        },
        plan.layout);
  }
  return plan.layout.containerSize;
}

Size OffsetView::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                         TextSystem& textSystem) const {
  ContainerMeasureScope scope(ctx);
  float const assignedWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedHeight = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  Size viewport{
      std::max(0.f, assignedWidth),
      std::max(0.f, assignedHeight),
  };
  if (viewport.width <= 0.f && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    viewport.width = constraints.maxWidth;
  }
  if (viewport.height <= 0.f && std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    viewport.height = constraints.maxHeight;
  }

  OffsetPlan const plan =
      planOffsetLayout(*this, constraints, viewport,
                       [&](Element const& child, LocalId, LayoutConstraints const& childConstraints) {
                         return child.measure(ctx, childConstraints, LayoutHints{}, textSystem);
                       });
  return plan.layout.contentSize;
}

Size ScaleAroundCenter::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                                TextSystem& textSystem) const {
  ContainerMeasureScope scope(ctx);
  float const assignedWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedHeight = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerWidth = std::max(0.f, assignedWidth);
  float innerHeight = std::max(0.f, assignedHeight);

  LayoutConstraints childConstraints = constraints;
  childConstraints.maxWidth = innerWidth > 0.f ? innerWidth : std::numeric_limits<float>::infinity();
  childConstraints.maxHeight = innerHeight > 0.f ? innerHeight : std::numeric_limits<float>::infinity();
  layout::clampLayoutMinToMax(childConstraints);

  return child.measure(ctx, childConstraints, LayoutHints{}, textSystem);
}

Size Spacer::measure(MeasureContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float minSize = 0.f;
  if (Element const* element = ctx.currentElement()) {
    minSize = std::max(0.f, element->minMainSize());
  }
  return {minSize, minSize};
}

namespace detail {

ComponentBuildResult buildMeasuredComponent(VStack const& stack, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  std::vector<std::unique_ptr<scenegraph::SceneNode>> existingChildren{};
  auto group = reuseGroupNode(ctx, std::move(existing), existingChildren);

  bool const widthAssigned = ctx.hasAssignedWidth() && build::zStackAxisStretches(ctx.hints().zStackHorizontalAlign);
  bool const heightAssigned = ctx.hasAssignedHeight() && build::zStackAxisStretches(ctx.hints().zStackVerticalAlign);
  float const assignedWidth = widthAssigned ? std::max(0.f, ctx.contentAssignedSize().width) : 0.f;
  float const assignedHeight = heightAssigned ? std::max(0.f, ctx.contentAssignedSize().height) : 0.f;
  std::optional<StackLayoutResult> cachedLayout{};
  if (MeasureLayoutCache* cache = ctx.measureLayoutCache()) {
    if (StackLayoutResult const* cached =
            cache->findStackLayout(ctx.makeMeasureLayoutKey(ctx.innerConstraints(), ctx.hints()))) {
      cachedLayout = *cached;
    }
  }
  VStackPlan const plan =
      planVStackLayout(stack, ctx.innerConstraints(), ctx.hints(), assignedWidth, heightAssigned,
                       assignedHeight, cachedLayout,
                       [&](Element const& child, LocalId local, LayoutConstraints const& childConstraints,
                           LayoutHints const& childHints) {
                         return ctx.measureChild(child, local, childConstraints, childHints);
                       });
  StackLayoutResult const& stackLayout = plan.layout;

  std::vector<std::unique_ptr<scenegraph::SceneNode>> nextChildren{};
  nextChildren.reserve(stack.children.size());
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LocalId const local = build::childLocalId(child, i);
    StackSlot const& slot = stackLayout.slots[i];
    LayoutConstraints childBuild = ctx.innerConstraints();
    childBuild.maxWidth = slot.assignedSize.width > 0.f ? slot.assignedSize.width
                                                        : std::numeric_limits<float>::infinity();
    childBuild.maxHeight = slot.assignedSize.height;
    childBuild.minHeight = child.minMainSize();
    clampLayoutMinToMax(childBuild);
    if (!plan.measuredChildSizes.empty()) {
      ctx.recordMeasuredSize(child, local, childBuild, plan.childHints, plan.measuredChildSizes[i]);
    }
    std::unique_ptr<scenegraph::SceneNode> childNode =
        ctx.buildChild(child, local, childBuild, plan.childHints,
                       Point{ctx.contentOrigin().x + slot.origin.x, ctx.contentOrigin().y + slot.origin.y},
                       slot.assignedSize, slot.assignedSize.width > 0.f, true,
                       takeExistingChild(existingChildren, i));
    childNode->setPosition(Point {
        childNode->position().x + slot.origin.x,
        childNode->position().y + slot.origin.y,
    });
    nextChildren.push_back(std::move(childNode));
  }
  group->replaceChildren(std::move(nextChildren));
  build::setAssignedGroupBounds(*group, stackLayout.containerSize);

  ComponentBuildResult result{};
  result.node = std::move(group);
  result.geometrySize = stackLayout.containerSize;
  result.hasGeometrySize = true;
  return result;
}

ComponentBuildResult buildMeasuredComponent(HStack const& stack, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  std::vector<std::unique_ptr<scenegraph::SceneNode>> existingChildren{};
  auto group = reuseGroupNode(ctx, std::move(existing), existingChildren);

  bool const widthAssigned = ctx.hasAssignedWidth() && build::zStackAxisStretches(ctx.hints().zStackHorizontalAlign);
  bool const heightAssigned = ctx.hasAssignedHeight() && build::zStackAxisStretches(ctx.hints().zStackVerticalAlign);
  float const assignedWidth =
      widthAssigned ? build::resolvedAssignedSpan(ctx.contentAssignedSize().width, ctx.hasAssignedWidth(),
                                                  ctx.innerConstraints().maxWidth)
                    : 0.f;
  float const assignedHeight = heightAssigned ? std::max(0.f, ctx.contentAssignedSize().height) : 0.f;
  bool const heightConstrained = heightAssigned;
  bool const widthConstrained =
      widthAssigned ||
      (build::zStackAxisStretches(ctx.hints().zStackHorizontalAlign) && stack.children.size() == 1 &&
       std::isfinite(ctx.innerConstraints().maxWidth) && ctx.innerConstraints().maxWidth > 0.f);
  std::optional<StackLayoutResult> cachedLayout{};
  if (MeasureLayoutCache* cache = ctx.measureLayoutCache()) {
    if (StackLayoutResult const* cached =
            cache->findStackLayout(ctx.makeMeasureLayoutKey(ctx.innerConstraints(), ctx.hints()))) {
      cachedLayout = *cached;
    }
  }
  HStackPlan const plan =
      planHStackLayout(stack, ctx.innerConstraints(), widthConstrained, assignedWidth, heightConstrained,
                       assignedHeight, cachedLayout,
                       [&](Element const& child, LocalId local, LayoutConstraints const& childConstraints) {
                         return ctx.measureChild(child, local, childConstraints, LayoutHints{});
                       },
                       [&](Element const& child, LocalId local, LayoutConstraints const& childConstraints,
                           LayoutHints const& rowHints) {
                         return ctx.measureChild(child, local, childConstraints, rowHints);
                       },
                       [] {});
  StackLayoutResult const& stackLayout = plan.layout;

  std::vector<std::unique_ptr<scenegraph::SceneNode>> nextChildren{};
  nextChildren.reserve(stack.children.size());
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LocalId const local = build::childLocalId(child, i);
    StackSlot const& slot = stackLayout.slots[i];
    LayoutConstraints childBuild = ctx.innerConstraints();
    childBuild.maxWidth = slot.assignedSize.width;
    childBuild.maxHeight = slot.assignedSize.height;
    childBuild.minWidth = child.minMainSize();
    clampLayoutMinToMax(childBuild);
    if (!plan.measuredChildSizes.empty()) {
      ctx.recordMeasuredSize(child, local, childBuild, plan.rowHints, plan.measuredChildSizes[i]);
    }
    std::unique_ptr<scenegraph::SceneNode> childNode =
        ctx.buildChild(child, local, childBuild, plan.rowHints,
                       Point{ctx.contentOrigin().x + slot.origin.x, ctx.contentOrigin().y + slot.origin.y},
                       slot.assignedSize, true, slot.assignedSize.height > 0.f,
                       takeExistingChild(existingChildren, i));
    childNode->setPosition(Point {
        childNode->position().x + slot.origin.x,
        childNode->position().y + slot.origin.y,
    });
    nextChildren.push_back(std::move(childNode));
  }
  group->replaceChildren(std::move(nextChildren));
  build::setAssignedGroupBounds(*group, stackLayout.containerSize);

  ComponentBuildResult result{};
  result.node = std::move(group);
  result.geometrySize = stackLayout.containerSize;
  result.hasGeometrySize = true;
  return result;
}

ComponentBuildResult buildMeasuredComponent(ZStack const& stack, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  std::vector<std::unique_ptr<scenegraph::SceneNode>> existingChildren{};
  auto group = reuseGroupNode(ctx, std::move(existing), existingChildren);

  float innerWidth =
      build::resolvedAssignedSpan(ctx.contentAssignedSize().width, ctx.hasAssignedWidth(), ctx.innerConstraints().maxWidth);
  float innerHeight =
      build::resolvedAssignedSpan(ctx.contentAssignedSize().height, ctx.hasAssignedHeight(), ctx.innerConstraints().maxHeight);
  std::optional<Size> cachedSize{};
  if (MeasureLayoutCache* cache = ctx.measureLayoutCache()) {
    if (Size const* cached = cache->findZStackSize(ctx.makeMeasureLayoutKey(ctx.innerConstraints(), ctx.hints()))) {
      cachedSize = *cached;
    }
  }
  ZStackPlan const plan =
      planZStackLayout(stack, ctx.innerConstraints(), Size{innerWidth, innerHeight}, cachedSize,
                       [&](Element const& child, LocalId local, LayoutConstraints const& childConstraints,
                           LayoutHints const& childHints) {
                         return ctx.measureChild(child, local, childConstraints, childHints);
                       });
  innerWidth = plan.containerSize.width;
  innerHeight = plan.containerSize.height;

  std::vector<std::unique_ptr<scenegraph::SceneNode>> nextChildren{};
  nextChildren.reserve(stack.children.size());
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LocalId const local = build::childLocalId(child, i);
    LayoutConstraints childBuild{};
    childBuild.maxWidth = innerWidth;
    childBuild.maxHeight = innerHeight;
    if (!plan.measuredChildSizes.empty()) {
      ctx.recordMeasuredSize(child, local, childBuild, plan.childHints, plan.measuredChildSizes[i]);
    }
    std::unique_ptr<scenegraph::SceneNode> childNode =
        ctx.buildChild(child, local, childBuild, plan.childHints, ctx.contentOrigin(), Size{innerWidth, innerHeight},
                       innerWidth > 0.f, innerHeight > 0.f,
                       takeExistingChild(existingChildren, i));
    childNode->setPosition(Point {
        childNode->position().x +
            hAlignOffset(childNode->bounds().width, innerWidth, stack.horizontalAlignment),
        childNode->position().y +
            vAlignOffset(childNode->bounds().height, innerHeight, stack.verticalAlignment),
    });
    nextChildren.push_back(std::move(childNode));
  }
  group->replaceChildren(std::move(nextChildren));
  build::setGroupBounds(*group, Size{innerWidth, innerHeight});

  ComponentBuildResult result{};
  result.node = std::move(group);
  result.geometrySize = Size{innerWidth, innerHeight};
  result.hasGeometrySize = true;
  return result;
}

ComponentBuildResult buildMeasuredComponent(Grid const& grid, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
  auto group = std::make_unique<scenegraph::GroupNode>();

  float const innerWidth =
      build::resolvedAssignedSpan(ctx.contentAssignedSize().width, ctx.hasAssignedWidth(), ctx.innerConstraints().maxWidth);
  float const innerHeight =
      build::resolvedAssignedSpan(ctx.contentAssignedSize().height, ctx.hasAssignedHeight(), ctx.innerConstraints().maxHeight);
  std::optional<GridLayoutResult> cachedLayout{};
  if (MeasureLayoutCache* cache = ctx.measureLayoutCache()) {
    if (GridLayoutResult const* cached =
            cache->findGridLayout(ctx.makeMeasureLayoutKey(ctx.innerConstraints(), ctx.hints()))) {
      cachedLayout = *cached;
    }
  }
  GridPlan const plan =
      planGridLayout(grid, ctx.innerConstraints(), innerWidth, innerHeight, cachedLayout,
                     [&](Element const& child, LocalId local, LayoutConstraints const& childConstraints) {
                       return ctx.measureChild(child, local, childConstraints, LayoutHints{});
                     });
  GridLayoutResult const& gridLayout = plan.layout;

  std::vector<std::unique_ptr<scenegraph::SceneNode>> nextChildren{};
  nextChildren.reserve(grid.children.size());
  for (std::size_t i = 0; i < grid.children.size(); ++i) {
    Element const& child = grid.children[i];
    LocalId const local = build::childLocalId(child, i);
    Rect const slot = gridLayout.slots[i];
    LayoutConstraints childBuild = gridChildConstraints(ctx.innerConstraints(), plan.metrics, i);
    childBuild.maxWidth = slot.width;
    childBuild.maxHeight = slot.height;
    clampLayoutMinToMax(childBuild);
    if (!plan.measuredChildSizes.empty()) {
      ctx.recordMeasuredSize(child, local, childBuild, LayoutHints{}, plan.measuredChildSizes[i]);
    }
    std::unique_ptr<scenegraph::SceneNode> childNode =
        ctx.buildChild(child, local, childBuild, LayoutHints{},
                       Point{ctx.contentOrigin().x + slot.x, ctx.contentOrigin().y + slot.y},
                       Size{slot.width, slot.height}, true, true);
    childNode->setPosition(Point {
        childNode->position().x + slot.x +
            hAlignOffset(childNode->bounds().width, slot.width, grid.horizontalAlignment),
        childNode->position().y + slot.y +
            vAlignOffset(childNode->bounds().height, slot.height, grid.verticalAlignment),
    });
    nextChildren.push_back(std::move(childNode));
  }
  group->replaceChildren(std::move(nextChildren));
  build::setGroupBounds(*group, gridLayout.containerSize);

  ComponentBuildResult result{};
  result.node = std::move(group);
  result.geometrySize = gridLayout.containerSize;
  result.hasGeometrySize = true;
  return result;
}

ComponentBuildResult buildMeasuredComponent(OffsetView const& offsetView, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
  auto group = std::make_unique<scenegraph::GroupNode>();

  Size const viewport{
      ctx.hasAssignedWidth() ? std::max(0.f, ctx.contentAssignedSize().width)
                             : std::max(0.f, assignedSpan(0.f, ctx.innerConstraints().maxWidth)),
      ctx.hasAssignedHeight() ? std::max(0.f, ctx.contentAssignedSize().height)
                              : std::max(0.f, assignedSpan(0.f, ctx.innerConstraints().maxHeight)),
  };
  if (offsetView.viewportSize.signal && !build::sizeApproximatelyEqual(*offsetView.viewportSize, viewport)) {
    offsetView.viewportSize.setSilently(viewport);
  }
  OffsetPlan const plan =
      planOffsetLayout(offsetView, ctx.innerConstraints(), viewport,
                       [&](Element const& child, LocalId local, LayoutConstraints const& childConstraints) {
                         return ctx.measureChild(child, local, childConstraints, LayoutHints{});
                       });
  ScrollContentLayout const& scrollLayout = plan.layout;
  Size const contentSize = scrollLayout.contentSize;
  if (offsetView.contentSize.signal && !build::sizeApproximatelyEqual(*offsetView.contentSize, contentSize)) {
    offsetView.contentSize.setSilently(contentSize);
  }

  std::vector<std::unique_ptr<scenegraph::SceneNode>> nextChildren{};
  nextChildren.reserve(offsetView.children.size());
  for (std::size_t i = 0; i < offsetView.children.size(); ++i) {
    Element const& child = offsetView.children[i];
    LocalId const local = build::childLocalId(child, i);
    ScrollChildSlot const& slot = scrollLayout.slots[i];
    LayoutConstraints childBuild = plan.childConstraints;
    if (slot.assignedSize.width > 0.f) {
      childBuild.maxWidth = slot.assignedSize.width;
    }
    if (slot.assignedSize.height > 0.f) {
      childBuild.maxHeight = slot.assignedSize.height;
    }
    clampLayoutMinToMax(childBuild);
    ctx.recordMeasuredSize(child, local, childBuild, LayoutHints{}, plan.measuredChildSizes[i]);
    std::unique_ptr<scenegraph::SceneNode> childNode =
        ctx.buildChild(child, local, childBuild, LayoutHints{},
                       Point{ctx.contentOrigin().x + slot.origin.x, ctx.contentOrigin().y + slot.origin.y},
                       slot.assignedSize, slot.assignedSize.width > 0.f,
                       slot.assignedSize.height > 0.f);
    childNode->setPosition(Point {
        childNode->position().x + slot.origin.x,
        childNode->position().y + slot.origin.y,
    });
    nextChildren.push_back(std::move(childNode));
  }
  group->replaceChildren(std::move(nextChildren));
  build::setGroupBounds(*group, contentSize);

  ComponentBuildResult result{};
  result.node = std::move(group);
  result.geometrySize = contentSize;
  result.hasGeometrySize = true;
  return result;
}

ComponentBuildResult buildMeasuredComponent(ScaleAroundCenter const& scaled, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
  auto transformNode = std::make_unique<scenegraph::GroupNode>();

  LocalId const childLocal = LocalId::fromString("$child");
  std::unique_ptr<scenegraph::SceneNode> childNode =
      ctx.buildChild(scaled.child, childLocal, ctx.innerConstraints(), LayoutHints {}, ctx.contentOrigin(),
                     ctx.contentAssignedSize(), ctx.hasAssignedWidth(), ctx.hasAssignedHeight());
  Size const geometrySize = build::rectSize(childNode->bounds());
  Rect const childVisualBounds = scenegraph::detail::subtreeLocalVisualBounds(*childNode);
  childNode->setPosition(Point {});
  Rect const childBounds = childNode->bounds();
  Point const pivot {childBounds.width * 0.5f, childBounds.height * 0.5f};
  transformNode->appendChild(std::move(childNode));
  transformNode->setTransform(
      Mat3::translate(pivot) * Mat3::scale(scaled.scale) *
      Mat3::translate(Point {-pivot.x, -pivot.y})
  );
  Rect const transformedVisualBounds =
      scenegraph::detail::transformBounds(transformNode->transform(), childVisualBounds);
  transformNode->setBounds(Rect::sharp(0.f, 0.f, transformedVisualBounds.width,
                                       transformedVisualBounds.height));

  ComponentBuildResult result{};
  result.node = std::move(transformNode);
  result.geometrySize = geometrySize;
  result.hasGeometrySize = true;
  return result;
}

ComponentBuildResult buildMeasuredComponent(Spacer const&, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
  ComponentBuildResult result{};
  result.node = std::make_unique<scenegraph::GroupNode>(build::sizeRect(ctx.paddedContentSize()));
  result.geometrySize = build::rectSize(result.node->bounds());
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
