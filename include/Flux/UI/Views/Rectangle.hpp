#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <functional>
#include <string>

namespace flux {

struct Rectangle {

  // ── Appearance ─────────────────────────────────────────────────────────────

  Rect frame{};
  CornerRadius cornerRadius{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();

  // ── Layout ─────────────────────────────────────────────────────────────────

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  // ── Interaction ────────────────────────────────────────────────────────────

  Cursor cursor = Cursor::Inherit;
  /// If true, this node is ignored for cursor resolution; the view behind is used (e.g. ScrollView overlay).
  /// Not on `Text` — text is always real content, not a full-bleed invisible capture layer.
  bool cursorPassthrough = false;
  /// When true, a PointerDown on this node claims focus for the window until another focusable node is tapped.
  bool focusable = false;

  // ── Events ─────────────────────────────────────────────────────────────────

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
