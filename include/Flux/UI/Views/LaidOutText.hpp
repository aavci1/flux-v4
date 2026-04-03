#pragma once

/// \file Flux/UI/Views/LaidOutText.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <memory>

namespace flux {

/// Pre-shaped text from `TextSystem::layout`; `origin` is overridden by the layout child frame when set.
struct LaidOutText : ViewModifiers<LaidOutText> {
  static constexpr bool memoizable = true;

  void layout(LayoutContext&) const;
  void renderFromLayout(RenderContext&, LayoutNode const&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  std::shared_ptr<TextLayout> textLayout;
  Point origin{};
};

} // namespace flux
