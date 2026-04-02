#pragma once

/// \file Flux/UI/Views/Rectangle.hpp
///
/// Filled and/or stroked rectangle primitive.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

/// Axis-aligned rounded rect leaf. \c width / \c height are explicit sizes; \c 0 means expand along
/// that axis from the proposed constraint box. \c offsetX / \c offsetY shift the rect within the
/// parent cell (e.g. ZStack overlays). Use \ref Element modifiers for interaction, events, corner
/// radius, flex, layer opacity, and translation.
struct Rectangle : ViewModifiers<Rectangle> {

  // ── Appearance ─────────────────────────────────────────────────────────────

  float offsetX = 0.f;
  float offsetY = 0.f;
  float width = 0.f;
  float height = 0.f;
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
};

} // namespace flux
