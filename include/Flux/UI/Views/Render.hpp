#pragma once

/// \file Flux/UI/Views/Render.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <functional>

namespace flux {

/// Built-in custom draw leaf for the retained scene builder and legacy render-component path.
/// Layout uses `measure`; painting uses `draw`. Set `pure=true` when drawing is deterministic for the
/// same inputs so node-local paint retention can persist across frames.
struct Render : ViewModifiers<Render> {
  std::function<Size(LayoutConstraints const&, LayoutHints const&)> measureFn{};
  std::function<void(Canvas&, Rect)> draw{};
  bool pure = false;

  void layout(LayoutContext& ctx) const;
  Size measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& textSystem) const;

  Size measure(LayoutConstraints const& constraints, LayoutHints const& hints) const {
    if (measureFn) {
      return measureFn(constraints, hints);
    }
    return {};
  }

  void render(Canvas& canvas, Rect frame) const {
    if (draw) {
      draw(canvas, frame);
    }
  }
};

} // namespace flux
