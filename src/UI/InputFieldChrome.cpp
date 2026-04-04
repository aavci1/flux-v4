#include <Flux/UI/InputFieldChrome.hpp>

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>

namespace flux {

ResolvedInputFieldChrome resolveInputFieldChrome(InputFieldChromeSpec const& spec, Theme const& theme) {
  return ResolvedInputFieldChrome{
      .textColor = resolveColor(spec.textColor, theme.colorTextPrimary),
      .placeholderColor = resolveColor(spec.placeholderColor, theme.colorTextPlaceholder),
      .backgroundColor = resolveColor(spec.backgroundColor, theme.colorSurfaceField),
      .borderColor = resolveColor(spec.borderColor, theme.colorBorder),
      .borderFocusColor = resolveColor(spec.borderFocusColor, theme.colorBorderFocus),
      .caretColor = resolveColor(spec.caretColor, theme.colorAccent),
      .selectionColor = resolveColor(spec.selectionColor, theme.colorAccentSubtle),
      .disabledColor = resolveColor(spec.disabledColor, theme.colorSurfaceDisabled),
      .borderWidth = resolveFloat(spec.borderWidth, 1.f),
      .borderFocusWidth = resolveFloat(spec.borderFocusWidth, 2.f),
      .cornerRadius = resolveFloat(spec.cornerRadius, theme.radiusMedium),
      .paddingH = resolveFloat(spec.paddingH, theme.paddingFieldH),
      .paddingV = resolveFloat(spec.paddingV, theme.paddingFieldV),
  };
}

ResolvedPickerFieldChrome resolvePickerFieldChrome(PickerFieldChromeSpec const& spec, Theme const& theme) {
  return ResolvedPickerFieldChrome{
      .input = resolveInputFieldChrome(spec.input, theme),
      .chevronColor = resolveColor(spec.chevronColor, theme.colorTextMuted),
      .rowHoverColor = resolveColor(spec.rowHoverColor, theme.colorSurfaceRowHover),
      .rowSelectedColor = resolveColor(spec.rowSelectedColor, theme.colorAccentSubtle),
  };
}

InputFieldDecoration applyOuterInputFieldDecoration(ResolvedInputFieldChrome const& chrome,
                                                    ElementModifiers const* outerMods) {
  FillStyle bgFill = FillStyle::solid(chrome.backgroundColor);
  StrokeStyle strokeN = StrokeStyle::solid(chrome.borderColor, chrome.borderWidth);
  StrokeStyle strokeF = StrokeStyle::solid(chrome.borderFocusColor, chrome.borderFocusWidth);
  CornerRadius cr{chrome.cornerRadius};
  if (outerMods) {
    if (!outerMods->fill.isNone()) {
      bgFill = outerMods->fill;
    }
    if (!outerMods->stroke.isNone()) {
      strokeN = outerMods->stroke;
      strokeF = StrokeStyle::solid(chrome.borderFocusColor, chrome.borderFocusWidth);
    }
    if (!outerMods->cornerRadius.isZero()) {
      cr = outerMods->cornerRadius;
    }
  }
  return InputFieldDecoration{.bgFill = std::move(bgFill),
                              .strokeNormal = std::move(strokeN),
                              .strokeFocus = std::move(strokeF),
                              .cornerRadius = cr};
}

} // namespace flux
