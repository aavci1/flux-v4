#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <functional>
#include <string>

namespace flux {

struct Rectangle {
  Rect frame{};
  CornerRadius cornerRadius{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  float flexGrow = 0.f;
  /// Defaults to 0 (unlike CSS `flex-shrink: 1`) so layout does not shrink views unless opted in.
  float flexShrink = 0.f;
  float minSize = 0.f;
  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
  std::function<void(Vec2)> onScroll;
  /// When true, a PointerDown on this node claims focus for the window until another focusable node is tapped.
  bool focusable = false;
  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(KeyCode, Modifiers)> onKeyUp;
  std::function<void(std::string const&)> onTextInput;

  Cursor cursor = Cursor::Inherit;
  /// If true, this node is ignored for cursor resolution; the view behind is used (e.g. ScrollView overlay).
  /// Not on `Text` — text is always real content, not a full-bleed invisible capture layer.
  bool cursorPassthrough = false;
};

} // namespace flux
