#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/HStack.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

Size HStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                     TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedHCross = stackMainAxisSpan(0.f, constraints.maxHeight);
  bool const heightConstrained = std::isfinite(assignedHCross) && assignedHCross > 0.f;
  LayoutConstraints childCs = constraints;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = heightConstrained ? assignedHCross : std::numeric_limits<float>::infinity();

  std::size_t const n = children.size();
  if (n == 1 && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    childCs.maxWidth = std::max(0.f, constraints.maxWidth);
  }

  if (n == 0) {
    return {0.f, 0.f};
  }

  std::vector<Size> sizes;
  sizes.reserve(n);
  for (Element const& ch : children) {
    Size const s = ch.measure(ctx, childCs, LayoutHints{}, ts);
    sizes.push_back(s);
  }

  float const assignedW = stackMainAxisSpan(0.f, constraints.maxWidth);

  std::vector<float> allocW(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocW[i] = std::max(sizes[i].width, children[i].minMainSize());
  }

  bool const widthConstrained = std::isfinite(assignedW) && assignedW > 0.f;
  if (widthConstrained && n > 0) {
    float const innerW = std::max(0.f, assignedW);
    float const gaps = n > 1 ? static_cast<float>(n - 1) * spacing : 0.f;
    float const targetSum = std::max(0.f, innerW - gaps);
    float sumNat = 0.f;
    for (float w : allocW) {
      sumNat += w;
    }
    float const extra = targetSum - sumNat;
    if (extra > kFlexEpsilon) {
      flexGrowAlongMainAxis(allocW, children, extra);
    } else if (extra < -kFlexEpsilon) {
      flexShrinkAlongMainAxis(allocW, children, targetSum);
    }
  } else if (n > 0) {
    warnFlexGrowIfParentMainAxisUnconstrained(children, widthConstrained);
  }

  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  float maxH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    LayoutConstraints cs2 = constraints;
    cs2.maxWidth = allocW[i];
    cs2.maxHeight = heightConstrained ? assignedHCross : std::numeric_limits<float>::infinity();
    LayoutHints rh{};
    rh.hStackCrossAlign = alignment;
    Size const s2 = children[i].measure(ctx, cs2, rh, ts);
    maxH = std::max(maxH, s2.height);
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  float outH = maxH;
  if (alignment == Alignment::Stretch && heightConstrained) {
    outH = std::max(outH, assignedHCross);
  }
  float outW = 0.f;
  if (n > 1) {
    outW += static_cast<float>(n - 1) * spacing;
  }
  for (float w : allocW) {
    outW += w;
  }
  if (widthConstrained) {
    outW = assignedW;
  }
  return {outW, outH};
}

} // namespace flux
