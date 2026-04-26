#pragma once

/// \file Flux/UI/Views/ScaleAroundCenter.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Element.hpp>

namespace flux {

/// Scales a single child around the center of the layout slot (used for press feedback).
struct ScaleAroundCenter : ViewModifiers<ScaleAroundCenter> {
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  float scale = 1.f;
  Element child;

  bool operator==(ScaleAroundCenter const& other) const {
    return scale == other.scale && child.typeTag() == other.child.typeTag();
  }
};

} // namespace flux
