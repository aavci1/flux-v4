#pragma once

/// \file Flux/UI/Views/ScaleAroundCenter.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Element.hpp>

namespace flux {

/// Scales a single child around the center of the layout slot (used for press feedback).
struct ScaleAroundCenter : ViewModifiers<ScaleAroundCenter> {
  void build(BuildContext&) const;
  Size measure(BuildContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  float scale = 1.f;
  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;
  Element child;
};

} // namespace flux
