#pragma once

/// \file Flux/UI/Views/Render.hpp
///
/// Custom draw leaf for the UI tree. Painting runs through the active Canvas renderer.

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <functional>

namespace flux {

struct Render : ViewModifiers<Render> {
  std::function<Size(LayoutConstraints const&, LayoutHints const&)> measureFn{};
  std::function<void(Canvas&, Rect)> draw{};
  bool pure = false;

  bool operator==(Render const& other) const {
    return static_cast<bool>(measureFn) == static_cast<bool>(other.measureFn) &&
           static_cast<bool>(draw) == static_cast<bool>(other.draw) && pure == other.pure;
  }

  Size measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
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
