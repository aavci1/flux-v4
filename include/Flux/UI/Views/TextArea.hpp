#pragma once

/// \file Flux/UI/Views/TextArea.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>
#include <string>
#include <vector>

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

    static Style plain() {
      return Style {
        .font = kFontFromTheme,
        .textColor = kFromTheme,
        .placeholderColor = kFromTheme,
        .backgroundColor = Colors::transparent,
        .borderColor = Colors::transparent,
        .borderFocusColor = Colors::transparent,
        .caretColor = kFromTheme,
        .selectionColor = kFromTheme,
        .disabledColor = kFromTheme,
        .borderWidth = 0.f,
        .borderFocusWidth = 0.f,
        .cornerRadius = 0.f,
        .paddingH = 0.f,
        .paddingV = 0.f,
      };
    }
  };

  State<std::string> value{};
  std::string placeholder;

  Style style{};

  TextAreaHeight height{};

  std::function<std::vector<AttributedRun>(std::string_view)> styler;

  bool disabled = false;
  int maxLength = 0;

  std::function<void(std::string const&)> onChange;
  std::function<void(std::string const&)> onEscape;

  Element body() const;
};

} // namespace flux
