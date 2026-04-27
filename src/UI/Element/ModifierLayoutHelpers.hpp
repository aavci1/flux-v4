#pragma once

#include <Flux/UI/LayoutEngine.hpp>

#include <cmath>
#include <optional>

namespace flux::detail {

inline bool modifierAxisStretches(std::optional<Alignment> const& alignment) {
  return alignment.has_value() && *alignment == Alignment::Stretch;
}

inline float modifierPositiveFinite(float value) {
  return std::isfinite(value) && value > 0.f ? value : 0.f;
}

inline float modifierStackAssignedWidth(LayoutConstraints const& constraints,
                                        LayoutHints const& hints) {
  float const assigned = modifierPositiveFinite(constraints.maxWidth);
  if (assigned <= 0.f) {
    return 0.f;
  }
  if (hints.hStackCrossAlign.has_value() || modifierAxisStretches(hints.vStackCrossAlign)) {
    return assigned;
  }
  return 0.f;
}

inline float modifierStackAssignedHeight(LayoutConstraints const& constraints,
                                         LayoutHints const& hints) {
  float const assigned = modifierPositiveFinite(constraints.maxHeight);
  if (assigned <= 0.f) {
    return 0.f;
  }
  if (hints.vStackCrossAlign.has_value() || modifierAxisStretches(hints.hStackCrossAlign)) {
    return assigned;
  }
  return 0.f;
}

inline float resolvedModifierWidth(LayoutConstraints const& constraints,
                                   LayoutHints const& hints,
                                   float explicitWidth) {
  float const assigned = modifierStackAssignedWidth(constraints, hints);
  return assigned > 0.f ? assigned : explicitWidth;
}

inline float resolvedModifierHeight(LayoutConstraints const& constraints,
                                    LayoutHints const& hints,
                                    float explicitHeight) {
  float const assigned = modifierStackAssignedHeight(constraints, hints);
  return assigned > 0.f ? assigned : explicitHeight;
}

} // namespace flux::detail
