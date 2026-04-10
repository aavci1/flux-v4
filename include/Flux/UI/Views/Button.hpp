#pragma once

/// \file Flux/UI/Views/Button.hpp
///
/// Part of the Flux public API.


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

struct Button : ViewModifiers<Button> {
  // ── Content ──────────────────────────────────────────────────────────────

  /// Button label text.
  std::string label;

  // ── Variant & appearance ─────────────────────────────────────────────────

  ButtonVariant variant = ButtonVariant::Primary;

  /// Override fixed height (`<= 0` or `kHeightFromTheme` = `Theme::controlHeightMedium`). Ignored for Link.
  float height = kHeightFromTheme;

  Font font = kFontFromTheme;

  /// Horizontal padding between label and button edge (`kFloatFromTheme` = `Theme::space4`).
  /// Ignored for Link (always 0).
  float paddingH = kFloatFromTheme;

  /// Uniform corner radius in points (`kFloatFromTheme` = `Theme::radiusMedium`). `body()` builds
  /// `CornerRadius` from this value — all four corners match. Per-corner radii (e.g. top-only sheet
  /// rounding) are not expressible on this field; use a lower-level primitive with explicit `CornerRadius`.
  float cornerRadius = kFloatFromTheme;

  /// Accent colour: Primary/Ghost/Secondary/Link label and focus ring.
  Color accentColor = kColorFromTheme;
  /// Danger colour: Destructive fill and focus ring.
  Color destructiveColor = kColorFromTheme;

  // ── Layout ───────────────────────────────────────────────────────────────
  // Flex: use chained `.flex(...)` on the `Element` from `body()`.

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
