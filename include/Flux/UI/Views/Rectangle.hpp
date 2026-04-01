#pragma once

/// \file Flux/UI/Views/Rectangle.hpp
///
/// Filled and/or stroked rectangle primitive with optional interaction handlers.

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <functional>
#include <string>

namespace flux {

/// Axis-aligned rounded rect leaf. \c width / \c height are explicit sizes; \c 0 means expand along
/// that axis from the proposed constraint box (see layout code). \c offsetX / \c offsetY shift the
/// rect within the parent cell (e.g. ZStack overlays).
struct Rectangle {

  // ── Appearance ─────────────────────────────────────────────────────────────

  float offsetX = 0.f;
  float offsetY = 0.f;
  float width = 0.f;
  float height = 0.f;
  CornerRadius cornerRadius{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();

  // ── Layout ─────────────────────────────────────────────────────────────────

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minMainSize = 0.f;

  // ── Interaction ────────────────────────────────────────────────────────────

  Cursor cursor = Cursor::Inherit;
  /// If true, this node is ignored for cursor resolution; the view behind is used (e.g. ScrollView overlay).
  /// Not on `Text` — text is always real content, not a full-bleed invisible capture layer.
  bool cursorPassthrough = false;
  /// When true, a PointerDown on this node claims focus for the window until another focusable node is tapped.
  bool focusable = false;

  // ── Events ─────────────────────────────────────────────────────────────────

  /// Coordinates are in the rectangle’s local space (origin top-left of the laid-out bounds).
  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(KeyCode, Modifiers)> onKeyUp;
  std::function<void(std::string const&)> onTextInput;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
  std::function<void(Vec2)> onScroll;
  std::function<void()> onTap;
};

} // namespace flux
