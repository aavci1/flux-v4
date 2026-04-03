#pragma once

/// \file Flux/UI/Views/TextArea.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>
#include <string>

namespace flux {

struct TextAreaHeight {
  float fixed = 0.f;
  float minIntrinsic = 80.f;
  float maxIntrinsic = 0.f;
};

struct TextArea : ViewModifiers<TextArea> {
  /// Visual tokens; any field may use \c kFromTheme / \c kFloatFromTheme to inherit from \ref Theme.
  /// Resolved in \c body() the same way as \ref Toggle::Style (see \c resolveStyle in \c TextArea.cpp).
  struct Style {
    Font font = kFontFromTheme;
    Color textColor = kFromTheme;
    Color placeholderColor = kFromTheme;
    Color backgroundColor = kFromTheme;
    Color borderColor = kFromTheme;
    Color borderFocusColor = kFromTheme;
    Color caretColor = kFromTheme;
    Color selectionColor = kFromTheme;
    Color disabledColor = kFromTheme;
    float borderWidth = kFloatFromTheme;
    float borderFocusWidth = kFloatFromTheme;
    float cornerRadius = kFloatFromTheme;
    float paddingH = kFloatFromTheme;
    float paddingV = kFloatFromTheme;
    float lineHeight = 0.f;
  };

  State<std::string> value{};
  std::string placeholder;

  /// **Modifier-first shell styling** (recommended): chain on the value returned from the initializer,
  /// e.g. `TextArea{ .value = v, .placeholder = "…", .height = {…} }.background(FillStyle::solid(…))
  /// .border(StrokeStyle::solid(…)).cornerRadius(CornerRadius{8.f}).clipContent(true).flex(1.f)`.
  /// Those override resolved \ref style tokens for the custom-draw chrome (`useOuterElementModifiers()` in
  /// \c body()). Optional \c .cursor() applies when not \c Inherit.
  ///
  /// **Layout**: outer \c .padding() and \c .flex() are standard \c Element modifiers on the wrapper (no struct
  /// flex fields — same as other views). Inner \c style.paddingH / \c style.paddingV remain **content** insets
  /// inside the border, not \c Element padding.

  Style style{};

  TextAreaHeight height{};

  bool disabled = false;
  int maxLength = 0;

  std::function<void(std::string const&)> onChange;
  std::function<void(std::string const&)> onEscape;

  Element body() const;
};

} // namespace flux
