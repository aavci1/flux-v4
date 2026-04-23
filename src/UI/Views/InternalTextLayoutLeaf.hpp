#pragma once

#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <memory>

namespace flux {

struct InternalTextLayoutLeaf : ViewModifiers<InternalTextLayoutLeaf> {
  std::shared_ptr<TextLayout const> layout;

  Size measure(MeasureContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
    ctx.advanceChildSlot();
    return layout ? layout->measuredSize : Size{};
  }
};

} // namespace flux
