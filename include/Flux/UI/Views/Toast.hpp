#pragma once

/// \file Flux/UI/Views/Toast.hpp
///
/// Part of the Flux public API.

#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/IconName.hpp>
#include <Flux/UI/Views/Button.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace flux {

enum class ToastTone : std::uint8_t {
  Neutral,
  Accent,
  Success,
  Warning,
  Danger,
};

enum class ToastPlacement : std::uint8_t {
  TopLeading,
  TopCenter,
  TopTrailing,
  BottomLeading,
  BottomCenter,
  BottomTrailing,
};

struct ToastAction {
  std::string label;
  ButtonVariant variant = ButtonVariant::Ghost;
  bool dismissOnTap = true;
  std::function<void()> action;
};

struct Toast {
  std::uint64_t id = 0;
  std::string title;
  std::string message;

  ToastTone tone = ToastTone::Neutral;
  ToastPlacement placement = ToastPlacement::BottomCenter;
  std::optional<IconName> icon;
  std::optional<ToastAction> action;

  bool showCloseButton = true;
  int autoDismissMs = 4000;

  float minWidth = 280.f;
  float maxWidth = 420.f;

  std::function<void()> onDismiss;
};

struct ToastOverlay : ViewModifiers<ToastOverlay> {
  std::vector<Toast> toasts;
  std::function<void(std::uint64_t)> onDismiss;

  Element body() const;
};

/// Hook: returns `(show, dismiss, clear, hasVisibleToasts)` for a non-modal toast overlay.
///
/// `show(toast)` appends a toast and returns the assigned id.
/// `dismiss(id)` removes one toast.
/// `clear()` removes all visible toasts.
/// `hasVisibleToasts` is true while any toast is currently shown.
std::tuple<std::function<std::uint64_t(Toast)>, std::function<void(std::uint64_t)>, std::function<void()>, bool>
useToast();

} // namespace flux
