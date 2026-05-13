#pragma once

/// \file Flux/UI/Views/BackdropBlur.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

/// A rectangular backdrop-filter region. The renderer samples already-rendered
/// window content, blurs it once per frame, then masks this region from that
/// blurred backdrop texture.
struct BackdropBlur : ViewModifiers<BackdropBlur> {
  float radius = 18.f;
  Color tint = Colors::transparent;
  CornerRadius corners{};

  bool operator==(BackdropBlur const& other) const {
    return radius == other.radius && tint == other.tint && corners == other.corners;
  }

  Element body() const;
};

} // namespace flux
