#pragma once

/// \file Flux/UI/Views/Render.hpp
///
/// Custom draw leaf for the UI tree. Painting runs through the active Canvas renderer.

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <functional>
#include <memory>

namespace flux {

class MountContext;
namespace scenegraph {
class SceneNode;
}

struct Render : ViewModifiers<Render> {
  /// Custom measurement callback. Return the retained leaf's desired size for the given constraints.
  std::function<Size(LayoutConstraints const&, LayoutHints const&)> measureFn{};
  /// Paint callback. Called with the node's local canvas and resolved frame.
  std::function<void(Canvas&, Rect)> draw{};
  /// Marks the draw callback as side-effect-free and replay-safe, so retained rendering may cache or
  /// reuse its output more aggressively. Set false when drawing depends on mutable external state,
  /// timers, randomness, or anything not captured by the retained node state itself.
  bool pure = false;

  bool operator==(Render const& other) const {
    return static_cast<bool>(measureFn) == static_cast<bool>(other.measureFn) &&
           static_cast<bool>(draw) == static_cast<bool>(other.draw) && pure == other.pure;
  }

  Size measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& textSystem) const;
  std::unique_ptr<scenegraph::SceneNode> mount(MountContext& ctx) const;

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
