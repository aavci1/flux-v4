#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>

namespace flux {

inline constexpr Font kFontFromTheme{.size = -1.f};

inline Font resolveFont(Font const& override, Font const& themeValue) {
  if (override.size < 0.f) {
    return themeValue;
  }
  return override;
}

struct FluxTheme {
  Color accent = Color::hex(0x3A7BD5);
  Color danger = Color::hex(0xD94040);

  Color surfaceField = Color::hex(0xFFFFFF);
  Color surfaceOverlay = Color::hex(0xFFFFFF);
  Color surfaceBackground = Color::hex(0xF2F2F7);
  Color surfaceHover = Color::hex(0xF8F8FA);
  /// Menu / list row hover (e.g. Picker dropdown).
  Color surfaceRowHover = Color::hex(0xF0F0F5);

  Color border = Color::hex(0xC8C8D0);
  Color borderSubtle = Color::hex(0xE0E0E6);

  Color textPrimary = Color::hex(0x111118);
  Color textSecondary = Color::hex(0x6E6E80);
  Color textPlaceholder = Color::hex(0xAAAAAA);
  Color textDisabled = Color::hex(0xAAAAAA);
  Color textMuted = Color::hex(0x8E8E9A);

  Color surfaceDisabled = Color::hex(0xDDDDDD);

  /// Modal / dimmed overlay scrim (e.g. alerts).
  Color overlayBackdropScrim = Color{0.f, 0.f, 0.f, 0.45f};
  /// Light dim behind non-modal popovers when using `kFromTheme` for `Popover::backdropColor`.
  Color overlayPopoverBackdrop = Color{0.f, 0.f, 0.f, 0.2f};

  Font fontBody = Font{.size = 15.f, .weight = 400.f};
  Font fontLabel = Font{.size = 14.f, .weight = 500.f};
  Font fontCaption = Font{.size = 13.f, .weight = 600.f};
  Font fontTitle = Font{.size = 17.f, .weight = 600.f};
  Font fontHeading = Font{.size = 22.f, .weight = 700.f};

  static FluxTheme light();
  static FluxTheme dark();
};

} // namespace flux
