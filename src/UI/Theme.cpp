#include <Flux/UI/Theme.hpp>

namespace flux {

FluxTheme FluxTheme::light() { return FluxTheme{}; }

FluxTheme FluxTheme::dark() {
  FluxTheme t;
  t.accent = Color::hex(0x5B9CF6);
  t.danger = Color::hex(0xFF6B6B);
  t.surfaceField = Color::hex(0x2C2C2E);
  t.surfaceOverlay = Color::hex(0x1C1C1E);
  t.surfaceBackground = Color::hex(0x000000);
  t.surfaceHover = Color::hex(0x3A3A3C);
  t.surfaceRowHover = Color::hex(0x48484A);
  t.border = Color::hex(0x3A3A3C);
  t.borderSubtle = Color::hex(0x2C2C2E);
  t.textPrimary = Color::hex(0xF2F2F7);
  t.textSecondary = Color::hex(0xAEAEB2);
  t.textPlaceholder = Color::hex(0x6E6E73);
  t.textDisabled = Color::hex(0x48484A);
  t.textMuted = Color::hex(0x8E8E93);
  t.surfaceDisabled = Color::hex(0x3A3A3C);
  t.overlayBackdropScrim = Color{0.f, 0.f, 0.f, 0.55f};
  t.overlayPopoverBackdrop = Color{0.f, 0.f, 0.f, 0.35f};
  return t;
}

} // namespace flux
