#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <limits>

namespace flux {

Size ScaleAroundCenter::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                                TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = constraints;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();
  layout::clampLayoutMinToMax(childCs);

  return child.measure(ctx, childCs, LayoutHints{}, ts);
}

} // namespace flux
