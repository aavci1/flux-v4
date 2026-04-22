#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/HStack.hpp>

#include "UI/Layout/Algorithms/StackLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"
#include "UI/SceneBuilder/MeasureLayoutCache.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

namespace {

bool zStackAxisStretches(std::optional<Alignment> alignment) {
  return !alignment || *alignment == Alignment::Stretch;
}

} // namespace

Size HStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                     TextSystem& ts) const {
  ComponentKey const layoutKey = ctx.currentElementKey();
  Element const* const currentElement = ctx.currentElement();
  ContainerMeasureScope scope(ctx);
  float const availableHeight = stackMainAxisSpan(0.f, constraints.maxHeight);
  bool const heightAssigned = zStackAxisStretches(hints.zStackVerticalAlign);
  float const assignedHCross = heightAssigned ? availableHeight : 0.f;
  bool const heightConstrained = heightAssigned && std::isfinite(assignedHCross) && assignedHCross > 0.f;
  bool const stretchCrossAxis = alignment == Alignment::Stretch && heightConstrained;
  LayoutConstraints childCs = constraints;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = stretchCrossAxis ? assignedHCross : std::numeric_limits<float>::infinity();

  std::size_t const n = children.size();
  if (n == 1 && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    childCs.maxWidth = std::max(0.f, constraints.maxWidth);
  }
  clampLayoutMinToMax(childCs);

  if (n == 0) {
    return {0.f, 0.f};
  }

  std::vector<Size> sizes;
  sizes.reserve(n);
  std::vector<StackMainAxisChild> stackChildren;
  stackChildren.reserve(n);
  for (Element const& ch : children) {
    Size const s = ch.measure(ctx, childCs, LayoutHints{}, ts);
    sizes.push_back(s);
    stackChildren.push_back(StackMainAxisChild{
        .naturalMainSize = s.width,
        .flexBasis = ch.flexBasis(),
        .minMainSize = ch.minMainSize(),
        .flexGrow = ch.flexGrow(),
        .flexShrink = ch.flexShrink(),
    });
  }

  float const availableWidth = stackMainAxisSpan(0.f, constraints.maxWidth);
  bool const widthAssigned = zStackAxisStretches(hints.zStackHorizontalAlign);
  float const assignedW = widthAssigned ? availableWidth : 0.f;
  bool const widthConstrained = widthAssigned && std::isfinite(assignedW) && assignedW > 0.f;
  if (!widthConstrained && n > 0) {
    warnFlexGrowIfParentMainAxisUnconstrained(children, widthConstrained);
  }
  StackMainAxisLayout const mainLayout =
      layoutStackMainAxis(stackChildren, spacing, assignedW, widthConstrained, justifyContent);

  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  float maxH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    LayoutConstraints cs2 = constraints;
    cs2.maxWidth = mainLayout.mainSizes[i];
    cs2.maxHeight = stretchCrossAxis ? assignedHCross : std::numeric_limits<float>::infinity();
    clampLayoutMinToMax(cs2);
    LayoutHints rh{};
    rh.hStackCrossAlign = alignment;
    Size const s2 = children[i].measure(ctx, cs2, rh, ts);
    maxH = std::max(maxH, s2.height);
    sizes[i] = s2;
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  StackLayoutResult const layoutResult =
      layoutStack(StackAxis::Horizontal, alignment, sizes, mainLayout.mainSizes,
                  mainLayout.itemSpacing, mainLayout.containerMainSize, mainLayout.startOffset, assignedHCross,
                  heightConstrained);
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

} // namespace flux
