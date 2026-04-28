#pragma once

/// \file Flux/UI/Views/ScaleAroundCenter.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Element.hpp>
#include <Flux/Reactive/Bindable.hpp>

namespace flux {

/// Scales a single child around the center of the layout slot (used for press feedback).
struct ScaleAroundCenter : ViewModifiers<ScaleAroundCenter> {
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;
  std::unique_ptr<scenegraph::SceneNode> mount(MountContext&) const;

  /// Uniform scale multiplier applied before per-axis overrides.
  Reactive::Bindable<float> scale{1.f};
  /// Horizontal scale multiplier.
  Reactive::Bindable<float> scaleX{1.f};
  /// Vertical scale multiplier.
  Reactive::Bindable<float> scaleY{1.f};
  /// Content to transform.
  Element child;

  bool operator==(ScaleAroundCenter const& other) const {
    return scale.isValue() && other.scale.isValue() && scale.value() == other.scale.value() &&
           scaleX.isValue() && other.scaleX.isValue() && scaleX.value() == other.scaleX.value() &&
           scaleY.isValue() && other.scaleY.isValue() && scaleY.value() == other.scaleY.value() &&
           child.typeTag() == other.child.typeTag();
  }
};

} // namespace flux
