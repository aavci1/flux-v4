#pragma once

/// \file Flux/UI/Views/VStack.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Alignment.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <vector>

namespace flux {

/// Vertical stack. Use **`.clipContent(bool)`** on the wrapping `Element` to clip children to bounds.
struct VStack : ViewModifiers<VStack> {
  void layout(LayoutContext&) const;
  void renderFromLayout(RenderContext&, LayoutNode const&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  float spacing = 8.f;
  Alignment alignment = Alignment::Center;
  std::vector<Element> children;
};

} // namespace flux
