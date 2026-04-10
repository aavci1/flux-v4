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

struct TextArea : ViewModifiers<TextArea> {
  // ── Types ──────────────────────────────────────────────────────────────────

  struct Style {
      Font font = kFontFromTheme;

      Color backgroundColor = kFromTheme;

      Color borderColor = kFromTheme;
      float borderWidth = kFloatFromTheme;

      float cornerRadius = kFloatFromTheme;

      EdgeInsets padding = EdgeInsets(0.f);

      Color textColor = kFromTheme;
      Color placeholderColor = kFromTheme;
      Color caretColor = kFromTheme;

      static TextArea::Style plain() {
          return TextArea::Style {
            .backgroundColor = Colors::transparent,
            .borderColor = Colors::transparent,
            .borderWidth = 0.f,
            .cornerRadius = 0.f
          };
      }
  };

  // ── State ──────────────────────────────────────────────────────────────────

  State<std::string> value {};

  // ── Properties ─────────────────────────────────────────────────────────────

  Style style;
  std::string placeholder;
  TextWrapping wrapping = TextWrapping::Wrap;
  bool disabled = false;

  std::optional<std::function<AttributedString(std::string_view)>> formatter;

  // ── Events ─────────────────────────────────────────────────────────────────

  std::function<void(std::string const&)> onChange;

  // ── Component protocol ─────────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
