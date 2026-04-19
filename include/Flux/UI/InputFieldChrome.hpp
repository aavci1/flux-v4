#pragma once

/// \file Flux/UI/InputFieldChrome.hpp
///
/// Shared theme resolution for field-style controls (e.g. \ref Picker trigger) — avoids duplicating
/// \c resolveColor / \c resolveFloat blocks. Shell decoration merged with \ref useOuterElementModifiers()
/// uses \ref applyOuterInputFieldDecoration.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

namespace flux {

struct Theme;
namespace detail {
struct ElementModifiers;
}

/// Raw tokens matching \ref TextInput field chrome (sentinels \c Color::theme() / \c kFloatFromTheme allowed).
struct InputFieldChromeSpec {
  Color textColor = Color::theme();
  Color placeholderColor = Color::theme();
  Color backgroundColor = Color::theme();
  Color borderColor = Color::theme();
  Color borderFocusColor = Color::theme();
  Color caretColor = Color::theme();
  Color selectionColor = Color::theme();
  Color disabledColor = Color::theme();
  float borderWidth = 1.f;
  float borderFocusWidth = 2.f;
  float cornerRadius = kFloatFromTheme;
  float paddingH = kFloatFromTheme;
  float paddingV = kFloatFromTheme;
};

/// Resolved field chrome (uniform corner radius; use \c CornerRadius{cornerRadius} when a struct is needed).
struct ResolvedInputFieldChrome {
  Color textColor;
  Color placeholderColor;
  Color backgroundColor;
  Color borderColor;
  Color borderFocusColor;
  Color caretColor;
  Color selectionColor;
  Color disabledColor;
  float borderWidth;
  float borderFocusWidth;
  float cornerRadius;
  float paddingH;
  float paddingV;
};

ResolvedInputFieldChrome resolveInputFieldChrome(InputFieldChromeSpec const& spec, Theme const& theme);

/// Picker adds menu/trigger colours on top of \ref InputFieldChromeSpec.
struct PickerFieldChromeSpec {
  InputFieldChromeSpec input{};
  Color chevronColor = Color::theme();
  Color rowHoverColor = Color::theme();
  Color rowSelectedColor = Color::theme();
};

struct ResolvedPickerFieldChrome {
  ResolvedInputFieldChrome input;
  Color chevronColor;
  Color rowHoverColor;
  Color rowSelectedColor;
};

ResolvedPickerFieldChrome resolvePickerFieldChrome(PickerFieldChromeSpec const& spec, Theme const& theme);

/// Merges resolved chrome with optional outer \c Element modifiers (e.g. \ref Picker trigger \c body()).
struct InputFieldDecoration {
  FillStyle bgFill;
  StrokeStyle strokeNormal;
  StrokeStyle strokeFocus;
  CornerRadius cornerRadius;
};

InputFieldDecoration applyOuterInputFieldDecoration(ResolvedInputFieldChrome const& chrome,
                                                    detail::ElementModifiers const* outerMods);

} // namespace flux
