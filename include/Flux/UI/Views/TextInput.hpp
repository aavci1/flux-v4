#pragma once

/// \file Flux/UI/Views/TextInput.hpp
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

struct TextInput : ViewModifiers<TextInput> {
  // ── Content ──────────────────────────────────────────────────────────────

  /// Binding to the text buffer. Caller owns this via useState<std::string>().
  State<std::string> value{};

  /// Shown when value is empty and the field is not focused.
  /// Shown (in placeholder colour) when value is empty and focused.
  std::string placeholder;

  // ── Appearance ───────────────────────────────────────────────────────────
  ///
  /// Field chrome defaults below apply when the control is not wrapped with outer
  /// `Element` modifiers. Chained `.background()`, `.border()`, and `.cornerRadius()` on
  /// `TextInput{…}` override `backgroundColor` / unfocused border / `cornerRadius` via
  /// `useOuterElementModifiers()` in `body()`.

  Font font = kFontFromTheme;

  Color textColor = kFromTheme;
  Color placeholderColor = kFromTheme;
  Color backgroundColor = kFromTheme;
  Color borderColor = kFromTheme;
  Color borderFocusColor = kFromTheme;
  Color caretColor = kFromTheme;
  Color selectionColor = kFromTheme;
  Color disabledColor = kFromTheme;

  float borderWidth = 1.f;
  float borderFocusWidth = 2.f;
  /// Uniform field corner radius (`kFloatFromTheme` = `Theme::radiusMedium`). Resolved to
  /// `CornerRadius` in `body()`; per-corner overrides require a different view, not this field.
  float cornerRadius = kFloatFromTheme;

  /// Total vertical height of the field. 0 = \ref resolvedInputFieldHeight (same default as \ref Picker).
  float height = 0.f;

  float paddingH = kFloatFromTheme;
  float paddingV = kFloatFromTheme;

  // ── Layout ───────────────────────────────────────────────────────────────
  // Flex / min main size: use chained `.flex(grow, shrink, minMain)` on the `Element` from `body()`.

  // ── Behaviour ──────────────────────────────────────────────────────────────

  bool disabled = false;

  /// Maximum number of UTF-8 characters accepted. 0 = unlimited.
  int maxLength = 0;

  /// Called after every change to value.
  std::function<void(std::string const&)> onChange;

  /// Called when Return is pressed.
  std::function<void(std::string const&)> onSubmit;

  // ── Component protocol ─────────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
