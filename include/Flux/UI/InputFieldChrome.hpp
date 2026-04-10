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
struct ElementModifiers;

/// Raw tokens matching \ref TextInput field chrome (sentinels \c kColorFromTheme / \c kFloatFromTheme allowed).
struct InputFieldChromeSpec {
  Color textColor = kColorFromTheme;
  Color placeholderColor = kColorFromTheme;
  Color backgroundColor = kColorFromTheme;
  Color borderColor = kColorFromTheme;
  Color borderFocusColor = kColorFromTheme;
  Color caretColor = kColorFromTheme;
  Color selectionColor = kColorFromTheme;
  Color disabledColor = kColorFromTheme;
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
  Color chevronColor = kColorFromTheme;
  Color rowHoverColor = kColorFromTheme;
  Color rowSelectedColor = kColorFromTheme;
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
                                                    ElementModifiers const* outerMods);

} // namespace flux
