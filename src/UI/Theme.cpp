#include <Flux/UI/Theme.hpp>

#include <algorithm>

namespace flux {

FluxTheme FluxTheme::light() { return FluxTheme{}; }

FluxTheme FluxTheme::dark() {
  FluxTheme t;

  t.colorAccent = Color::hex(0x5B9CF6);
  t.colorOnAccent = Color::hex(0x000000);
  t.colorAccentSubtle = Color{0.36f, 0.61f, 0.97f, 0.15f};

  t.colorDanger = Color::hex(0xFF6B6B);
  t.colorOnDanger = Color::hex(0x000000);
  t.colorDangerSubtle = Color{1.f, 0.42f, 0.42f, 0.15f};

  t.colorSuccess = Color::hex(0x4ADE80);
  t.colorOnSuccess = Color::hex(0x000000);
  t.colorSuccessSubtle = Color{0.29f, 0.87f, 0.50f, 0.15f};

  t.colorWarning = Color::hex(0xFBBF24);
  t.colorOnWarning = Color::hex(0x000000);
  t.colorWarningSubtle = Color{0.98f, 0.75f, 0.14f, 0.15f};

  t.colorBackground = Color::hex(0x000000);
  t.colorSurface = Color::hex(0x1C1C1E);
  t.colorSurfaceOverlay = Color::hex(0x2C2C2E);
  t.colorSurfaceField = Color::hex(0x2C2C2E);
  t.colorSurfaceHover = Color::hex(0x3A3A3C);
  t.colorSurfaceRowHover = Color::hex(0x48484A);
  t.colorSurfaceDisabled = Color::hex(0x3A3A3C);

  t.colorBorder = Color::hex(0x48484A);
  t.colorBorderSubtle = Color::hex(0x3A3A3C);
  t.colorBorderFocus = Color::hex(0x5B9CF6);

  t.colorTextPrimary = Color::hex(0xF2F2F7);
  t.colorTextSecondary = Color::hex(0xAEAEB2);
  t.colorTextMuted = Color::hex(0x8E8E93);
  t.colorTextPlaceholder = Color::hex(0x6E6E73);
  t.colorTextDisabled = Color::hex(0x48484A);
  t.colorTextOnAccent = Color::hex(0x000000);

  t.colorScrimModal = Color{0.f, 0.f, 0.f, 0.60f};
  t.colorScrimPopover = Color{0.f, 0.f, 0.f, 0.f};

  t.shadowColor = Color{0.f, 0.f, 0.f, 0.35f};

  return t;
}

FluxTheme FluxTheme::compact() { return FluxTheme::light().withDensity(0.75f); }

FluxTheme FluxTheme::comfortable() { return FluxTheme::light().withDensity(1.25f); }

namespace {

// Nominal 8 pt grid at density = 1.0 (matches `FluxTheme` defaults in Theme.hpp).
inline constexpr float kSpace1 = 4.f;
inline constexpr float kSpace2 = 8.f;
inline constexpr float kSpace3 = 12.f;
inline constexpr float kSpace4 = 16.f;
inline constexpr float kSpace5 = 20.f;
inline constexpr float kSpace6 = 24.f;
inline constexpr float kSpace7 = 32.f;
inline constexpr float kSpace8 = 48.f;

} // namespace

FluxTheme FluxTheme::withDensity(float d) const {
  FluxTheme t = *this;
  t.density = d;
  // Scale the full space scale so components using `theme.spaceN` (Button, layouts, etc.)
  // participate in compact/comfortable, not only paddingFieldH/V.
  // radius* and typography tokens are left unchanged; motion/icon tokens unchanged.
  t.space1 = kSpace1 * d;
  t.space2 = kSpace2 * d;
  t.space3 = kSpace3 * d;
  t.space4 = kSpace4 * d;
  t.space5 = kSpace5 * d;
  t.space6 = kSpace6 * d;
  t.space7 = kSpace7 * d;
  t.space8 = kSpace8 * d;
  t.paddingFieldH = t.space3;
  t.paddingFieldV = t.space2;
  t.controlHeightSmall = std::max(24.f, 28.f * d);
  t.controlHeightMedium = std::max(28.f, 36.f * d);
  t.controlHeightLarge = std::max(36.f, 44.f * d);
  return t;
}

} // namespace flux
