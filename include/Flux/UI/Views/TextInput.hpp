#pragma once

/// \file Flux/UI/Views/TextInput.hpp
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

namespace flux {

struct TextInput : ViewModifiers<TextInput> {
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
    float height = 0.f;

    static Style plain() {
      return Style{.backgroundColor = Colors::transparent,
                   .borderColor = Colors::transparent,
                   .borderFocusColor = Colors::transparent,
                   .borderWidth = 0.f,
                   .borderFocusWidth = 0.f,
                   .cornerRadius = 0.f,
                   .paddingH = 0.f,
                   .paddingV = 0.f};
    }
  };

  State<std::string> value{};
  std::string placeholder;

  std::function<std::vector<AttributedRun>(std::string_view)> styler;

  Style style{};

  bool disabled = false;
  int maxLength = 0;

  std::function<void(std::string const&)> onChange;
  std::function<void(std::string const&)> onSubmit;

  Element body() const;
};

} // namespace flux
