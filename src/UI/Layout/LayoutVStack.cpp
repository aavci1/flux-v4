#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include "UI/Layout/Algorithms/StackLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

Size VStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                     TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float innerW = std::max(0.f, assignedW);

  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  clampLayoutMinToMax(childCs);
  LayoutHints childHints{};
  childHints.vStackCrossAlign = alignment;

  std::vector<Size> sizes;
  sizes.reserve(children.size());
  std::vector<StackMainAxisChild> stackChildren;
  stackChildren.reserve(children.size());
  std::size_t n = children.size();
  for (Element const& ch : children) {
    Size const s = ch.measure(ctx, childCs, childHints, ts);
    sizes.push_back(s);
    stackChildren.push_back(StackMainAxisChild{
        .naturalMainSize = s.height,
        .minMainSize = ch.minMainSize(),
        .flexGrow = ch.flexGrow(),
        .flexShrink = ch.flexShrink(),
    });
  }

  float const assignedH = stackMainAxisSpan(0.f, constraints.maxHeight);
  bool const heightConstrained = std::isfinite(assignedH) && assignedH > 0.f;
  if (!heightConstrained && n > 0) {
    warnFlexGrowIfParentMainAxisUnconstrained(children, heightConstrained);
  }

  StackMainAxisLayout const mainLayout =
      layoutStackMainAxis(stackChildren, spacing, assignedH, heightConstrained);
  StackLayoutResult const layoutResult =
      layoutStack(StackAxis::Vertical, alignment, sizes, mainLayout.mainSizes,
                  spacing, mainLayout.containerMainSize, mainLayout.startOffset, assignedW,
                  assignedW > 0.f);
  return layoutResult.containerSize;
}

} // namespace flux
