#pragma once

#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Hooks.hpp>

#include <Flux/Core/Types.hpp>

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

struct Alert {
  // ── Content ──────────────────────────────────────────────────────────────

  std::string title;
  /// Optional; empty = no message row.
  std::string message;

  /// Up to three buttons, rendered left-to-right (last = rightmost = primary).
  /// When empty, a single "OK" Secondary button is added automatically.
  std::vector<AlertButton> buttons;

  // ── Appearance ───────────────────────────────────────────────────────────

  /// Width of the card. 360 pt matches macOS alert width convention.
  float cardWidth = 360.f;

  Color cardColor = Color::hex(0xFFFFFF);
  Color cardStrokeColor = Color::hex(0xE0E0E6);
  Color titleColor = Color::hex(0x111118);
  Color messageColor = Color::hex(0x6E6E80);
  Color backdropColor = Color{0.f, 0.f, 0.f, 0.45f};
  CornerRadius cornerRadius{14.f};

  // ── Behaviour ────────────────────────────────────────────────────────────

  /// When true, pressing Escape dismisses without calling any button action.
  bool dismissOnEscape = true;

  // ── Component protocol ───────────────────────────────────────────────────

  /// body() is not the primary API. Use show() / hide() / useAlert() instead.
  Element body() const;

private:
  std::vector<Element> buildContent() const;
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
