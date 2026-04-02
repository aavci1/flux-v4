#pragma once

/// \file Flux/UI/Views/Alert.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Hooks.hpp>

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>
#include <string>
#include <tuple>
#include <vector>

namespace flux {

struct AlertButton {
  std::string label;
  ButtonVariant variant = ButtonVariant::Secondary;
  bool disabled = false;
  /// Called when this button is tapped or activated by keyboard.
  /// The alert is dismissed automatically before this is called.
  std::function<void()> action;
};

struct Alert : ViewModifiers<Alert> {
  // ── Content ──────────────────────────────────────────────────────────────

  std::string title;
  /// Optional; empty = no message row.
  std::string message;

  /// Up to three buttons, rendered left-to-right (last = rightmost = primary).
  /// When empty, a single "OK" Secondary button is added automatically.
  /// `useAlert` keeps only the first three if more are supplied.
  std::vector<AlertButton> buttons;

  // ── Appearance ───────────────────────────────────────────────────────────

  /// Width of the card. 360 pt matches macOS alert width convention.
  float cardWidth = 360.f;

  Color cardColor = kFromTheme;
  Color cardStrokeColor = kFromTheme;
  Color titleColor = kFromTheme;
  Color messageColor = kFromTheme;
  Color backdropColor = kFromTheme;
  /// Uniform card corner radius (`kFloatFromTheme` = `FluxTheme::radiusXLarge`). Not a `CornerRadius`
  /// struct field — all corners share one value; asymmetric cards need a custom element.
  float cornerRadius = kFloatFromTheme;

  // ── Behaviour ────────────────────────────────────────────────────────────

  /// When true, pressing Escape dismisses without calling any button action.
  bool dismissOnEscape = true;

  // ── Component protocol ───────────────────────────────────────────────────

  /// body() is not the primary API. Use show() / hide() / useAlert() instead.
  Element body() const;

private:
  std::vector<Element> buildContent(Color titleC, Color msgC, FluxTheme const& theme) const;
};

/// Hook: returns (show, hide, isPresented) for presenting an Alert.
///
/// show(alert) — pushes the alert as a modal overlay.
/// hide()      — dismisses the alert.
/// isPresented — true while the alert is on screen.
///
/// Alert uses useOverlay internally; the overlay config is managed here.
/// Must be called inside body() like other hooks.
std::tuple<std::function<void(Alert)>, std::function<void()>, bool> useAlert();

} // namespace flux
