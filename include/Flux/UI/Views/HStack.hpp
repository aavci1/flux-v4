#pragma once

/// \file Flux/UI/Views/HStack.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Alignment.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <vector>

namespace flux {

/// Horizontal stack. Use **`.padding(float)`** / **`.clipContent(bool)`** on the wrapping `Element` for
/// inset and clipping.
struct HStack : ViewModifiers<HStack> {
  void layout(LayoutContext&) const;
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  float spacing = 8.f;
  Alignment alignment = Alignment::Center;
  std::vector<Element> children;
};

} // namespace flux
