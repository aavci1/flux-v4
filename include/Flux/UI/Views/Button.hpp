#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>

#include <functional>
#include <string>

namespace flux {

enum class ButtonVariant : std::uint8_t {
  Primary,
  Secondary,
  Destructive,
  Ghost,
  Link,
};

struct Button {
  // ── Content ──────────────────────────────────────────────────────────────

  /// Button label text.
  std::string label;

  // ── Variant & appearance ─────────────────────────────────────────────────

  ButtonVariant variant = ButtonVariant::Primary;

  /// Override fixed height (`<= 0` or `kHeightFromTheme` = `FluxTheme::controlHeightMedium`). Ignored for Link.
  float height = kHeightFromTheme;

  Font font = kFontFromTheme;

  /// Horizontal padding between label and button edge (`kFloatFromTheme` = `FluxTheme::space4`).
  /// Ignored for Link (always 0).
  float paddingH = kFloatFromTheme;

  /// Uniform corner radius (`kFloatFromTheme` = `FluxTheme::radiusMedium`).
  float cornerRadius = kFloatFromTheme;

  /// Accent colour: Primary/Ghost/Secondary/Link label and focus ring.
  Color accentColor = kFromTheme;
  /// Danger colour: Destructive fill and focus ring.
  Color destructiveColor = kFromTheme;

  // ── Layout ───────────────────────────────────────────────────────────────

  /// Always 0 for Link (ignored if set).
  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  // ── Behaviour ────────────────────────────────────────────────────────────

  bool disabled = false;

  /// Primary tap handler.
  std::function<void()> onTap;

  /// Optional: name of a window action. The button reads
  /// `Window::isActionEnabled(actionName)` to derive its own enabled state,
  /// and registers `useWindowAction` so the shortcut fires the same handler.
  std::string actionName;

  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
