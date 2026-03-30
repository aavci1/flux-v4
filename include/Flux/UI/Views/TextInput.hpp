#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>

#include <functional>
#include <string>

namespace flux {

struct TextInput {
  // ── Content ──────────────────────────────────────────────────────────────

  /// Binding to the text buffer. Caller owns this via useState<std::string>().
  State<std::string> value{};

  /// Shown when value is empty and the field is not focused.
  /// Shown (in placeholder colour) when value is empty and focused.
  std::string placeholder;

  // ── Appearance ───────────────────────────────────────────────────────────

  Font font{ .size = 15.f, .weight = 400.f };

  Color textColor = Color::hex(0x111118);
  Color placeholderColor = Color::hex(0xAAAAAA);
  Color backgroundColor = Color::hex(0xFFFFFF);
  Color borderColor = Color::hex(0xC8C8D0);
  Color borderFocusColor = Color::hex(0x3A7BD5);
  Color caretColor = Color::hex(0x3A7BD5);
  Color selectionColor = Color{0.23f, 0.48f, 0.84f, 0.25f};
  Color disabledColor = Color::hex(0xDDDDDD);

  float borderWidth = 1.f;
  float borderFocusWidth = 2.f;
  CornerRadius cornerRadius{8.f};

  /// Total vertical height of the field. 0 = \ref resolvedInputFieldHeight (same default as \ref Picker).
  float height = 0.f;

  float paddingH = 12.f;
  float paddingV = 8.f;

  // ── Layout ───────────────────────────────────────────────────────────────

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

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
