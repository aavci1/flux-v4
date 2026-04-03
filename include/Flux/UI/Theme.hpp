#pragma once

/// \file Flux/UI/Theme.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>

#include <string>

namespace flux {

inline constexpr Font kFontFromTheme{.size = -1.f};

Font resolveFont(Font const& override, Font const& themeValue);

/// Raw colour palette — 50 named swatches across 5 hues × 10 steps.
/// Used to build FluxTheme presets; not consumed by components directly.
/// Step convention: 50 = lightest, 950 = darkest (same as Tailwind).
struct FluxPalette {
  Color blue50 = Color::hex(0xEFF6FF);
  Color blue100 = Color::hex(0xDBEAFE);
  Color blue200 = Color::hex(0xBFD7FE);
  Color blue300 = Color::hex(0x93BBFD);
  Color blue400 = Color::hex(0x6098FA);
  Color blue500 = Color::hex(0x3A7BD5);
  Color blue600 = Color::hex(0x2563EB);
  Color blue700 = Color::hex(0x1D4ED8);
  Color blue800 = Color::hex(0x1E3FA8);
  Color blue900 = Color::hex(0x1E3A8A);

  Color red50 = Color::hex(0xFFF1F2);
  Color red100 = Color::hex(0xFFE4E6);
  Color red200 = Color::hex(0xFECDD3);
  Color red300 = Color::hex(0xFCA5A5);
  Color red400 = Color::hex(0xF87171);
  Color red500 = Color::hex(0xD94040);
  Color red600 = Color::hex(0xDC2626);
  Color red700 = Color::hex(0xB91C1C);
  Color red800 = Color::hex(0x991B1B);
  Color red900 = Color::hex(0x7F1D1D);

  Color green50 = Color::hex(0xF0FDF4);
  Color green100 = Color::hex(0xDCFCE7);
  Color green200 = Color::hex(0xBBF7D0);
  Color green300 = Color::hex(0x86EFAC);
  Color green400 = Color::hex(0x4ADE80);
  Color green500 = Color::hex(0x22C55E);
  Color green600 = Color::hex(0x16A34A);
  Color green700 = Color::hex(0x15803D);
  Color green800 = Color::hex(0x166534);
  Color green900 = Color::hex(0x14532D);

  Color amber50 = Color::hex(0xFFFBEB);
  Color amber100 = Color::hex(0xFEF3C7);
  Color amber200 = Color::hex(0xFDE68A);
  Color amber300 = Color::hex(0xFCD34D);
  Color amber400 = Color::hex(0xFBBF24);
  Color amber500 = Color::hex(0xF59E0B);
  Color amber600 = Color::hex(0xD97706);
  Color amber700 = Color::hex(0xB45309);
  Color amber800 = Color::hex(0x92400E);
  Color amber900 = Color::hex(0x78350F);

  Color neutral50 = Color::hex(0xF8F8FA);
  Color neutral100 = Color::hex(0xF2F2F7);
  Color neutral200 = Color::hex(0xE5E5EA);
  Color neutral300 = Color::hex(0xC8C8D0);
  Color neutral400 = Color::hex(0xAAAAAA);
  Color neutral500 = Color::hex(0x8E8E9A);
  Color neutral600 = Color::hex(0x6E6E80);
  Color neutral700 = Color::hex(0x48484A);
  Color neutral800 = Color::hex(0x3A3A3C);
  Color neutral900 = Color::hex(0x1C1C1E);
  Color neutral950 = Color::hex(0x111118);
};

struct FluxTheme {
  Color colorAccent = Color::hex(0x3A7BD5);
  Color colorOnAccent = Color::hex(0xFFFFFF);
  Color colorAccentSubtle = Color{0.23f, 0.48f, 0.84f, 0.12f};

  Color colorDanger = Color::hex(0xD94040);
  Color colorOnDanger = Color::hex(0xFFFFFF);
  Color colorDangerSubtle = Color{0.85f, 0.25f, 0.25f, 0.12f};

  Color colorSuccess = Color::hex(0x16A34A);
  Color colorOnSuccess = Color::hex(0xFFFFFF);
  Color colorSuccessSubtle = Color{0.09f, 0.64f, 0.29f, 0.12f};

  Color colorWarning = Color::hex(0xD97706);
  Color colorOnWarning = Color::hex(0xFFFFFF);
  Color colorWarningSubtle = Color{0.85f, 0.47f, 0.02f, 0.12f};

  Color colorBackground = Color::hex(0xF2F2F7);
  Color colorSurface = Color::hex(0xFFFFFF);
  Color colorSurfaceOverlay = Color::hex(0xFFFFFF);
  Color colorSurfaceField = Color::hex(0xFFFFFF);
  Color colorSurfaceHover = Color::hex(0xF8F8FA);
  Color colorSurfaceRowHover = Color::hex(0xF0F0F5);
  Color colorSurfaceDisabled = Color::hex(0xE5E5EA);

  Color colorBorder = Color::hex(0xC8C8D0);
  Color colorBorderSubtle = Color::hex(0xE5E5EA);
  Color colorBorderFocus = Color::hex(0x7ABBF5);

  Color colorTextPrimary = Color::hex(0x111118);
  Color colorTextSecondary = Color::hex(0x6E6E80);
  Color colorTextMuted = Color::hex(0x8E8E9A);
  Color colorTextPlaceholder = Color::hex(0xAAAAAA);
  Color colorTextDisabled = Color::hex(0xAAAAAA);
  Color colorTextOnAccent = Color::hex(0xFFFFFF);

  Color colorScrimModal = Color{0.f, 0.f, 0.f, 0.45f};
  Color colorScrimPopover = Color{0.f, 0.f, 0.f, 0.f};

  TextStyle typeDisplay{34.f, 700.f, 1.12f};
  TextStyle typeHeading{22.f, 700.f, 1.18f};
  TextStyle typeTitle{17.f, 600.f, 1.24f};
  TextStyle typeSubtitle{15.f, 600.f, 1.28f};
  TextStyle typeBody{15.f, 400.f, 1.40f};
  TextStyle typeBodySmall{13.f, 400.f, 1.40f};
  TextStyle typeLabel{14.f, 500.f, 1.20f};
  TextStyle typeLabelSmall{12.f, 500.f, 1.20f};
  TextStyle typeCode{13.f, 400.f, 1.50f};

  // Spacing scale (8 pt grid). At density 1.0, space3 is 12 pt, space4 is 16 pt, etc.
  // `withDensity(d)` scales space1–space8 by d (and updates paddingFieldH/V to match space3/space2).
  // After compact/comfortable, spaceN are no longer fixed “name = 12 pt” constants — always read from
  // `FluxTheme` so layout participates in density; hardcoded literals (e.g. 24.f) do not.
  float space1 = 4.f;
  float space2 = 8.f;
  float space3 = 12.f;
  float space4 = 16.f;
  float space5 = 20.f;
  float space6 = 24.f;
  float space7 = 32.f;
  float space8 = 48.f;

  /// Multiplier recorded when using `withDensity`; spacing fields above include its effect after `withDensity`.
  float density = 1.0f;

  /// Horizontal / vertical padding for single-line fields; kept in sync with space3 / space2 by `withDensity`.
  float paddingFieldH = 12.f;
  float paddingFieldV = 8.f;

  // Corner radii (points). Not scaled by `withDensity` — shape stays stable across density presets.
  float radiusNone = 0.f;
  float radiusXSmall = 4.f;
  float radiusSmall = 6.f;
  float radiusMedium = 8.f;
  float radiusLarge = 10.f;
  float radiusXLarge = 14.f;
  float radiusFull = 9999.f;

  float controlHeightSmall = 28.f;
  float controlHeightMedium = 36.f;
  float controlHeightLarge = 44.f;

  float durationInstant = 0.00f;
  float durationFast = 0.10f;
  float durationMedium = 0.18f;
  float durationSlow = 0.30f;

  bool reducedMotion = false;

  // Toggle
  float toggleTrackWidth = 44.f;
  float toggleTrackHeight = 26.f;
  float toggleThumbInset = 4.f;
  float toggleBorderWidth = 1.f;
  float toggleThumbBorderWidth = 0.f;
  Color toggleOnColor = Color::hex(0x3A7BD5);
  Color toggleOffColor = Color::hex(0xD7D7D7);
  Color toggleThumbColor = Color::hex(0xFFFFFF);
  Color toggleThumbBorderColor = Color::hex(0xFFFFFF);
  Color toggleBorderColor = Color::hex(0xE5E5E5);

  // Checkbox
  float checkboxBoxSize = 20.f;
  float checkboxCornerRadius = 4.f;
  float checkboxBorderWidth = 2.0f;
  Color checkboxCheckedColor = Color::hex(0x3A7BD5);
  Color checkboxUncheckedColor = Color::hex(0xD7D7D7);
  Color checkboxCheckColor = Color::hex(0xFFFFFF);
  Color checkboxBorderColor = Color::hex(0xEEEEEE);

  // Slider
  float sliderTrackHeight = 4.f;
  float sliderThumbSize = 20.f;
  float sliderThumbBorderWidth = 2.f;
  Color sliderTrackColor = Color::hex(0xE5E5E5);
  Color sliderThumbColor = Color::hex(0xFFFFFF);
  Color sliderThumbBorderColor = Color::hex(0xEEEEEE);

  /// Bundled Material Symbols Rounded (override to swap icon sets globally).
  std::string iconFontFamily = "Material Symbols Rounded";

  Color shadowColor = Color{0.f, 0.f, 0.f, 0.15f};
  /// Drop shadow blur radius (logical px) for control thumbs, buttons, etc.
  float shadowRadiusControl = 6.f;
  /// Drop shadow vertical offset (logical px); positive = downward.
  float shadowOffsetYControl = 0.f;
  /// Popover / tooltip card shadow (path fill uses offset pass; radius reserved for future blur).
  float shadowRadiusPopover = 4.f;
  float shadowOffsetYPopover = 3.f;

  static FluxTheme light();
  static FluxTheme dark();
  static FluxTheme compact();
  static FluxTheme comfortable();

  FluxTheme withDensity(float d) const;
};

} // namespace flux
