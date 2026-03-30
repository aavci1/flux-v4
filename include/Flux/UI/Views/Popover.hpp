#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/Views/PopoverPlacement.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

#include <functional>
#include <optional>
#include <tuple>

namespace flux {

struct Popover {
  // ── Content ──────────────────────────────────────────────────────────────

  /// The content to display inside the popover card.
  Element content{Rectangle{}};

  // ── Placement ──────────────────────────────────────────────────────────────

  PopoverPlacement placement = PopoverPlacement::Below;

  /// Gap between the anchor edge and the popover card (before the arrow).
  float gap = 6.f;

  /// When true, a callout triangle is drawn pointing at the anchor.
  bool arrow = true;

  // ── Appearance ───────────────────────────────────────────────────────────

  Color backgroundColor = Color::hex(0xFFFFFF);
  Color borderColor = Color::hex(0xE0E0E6);
  float borderWidth = 1.f;
  CornerRadius cornerRadius{10.f};

  /// Inset between the card outline and the popover content (default matches system padding).
  float contentPadding = 12.f;

  /// Maximum size of the popover content area (excluding arrow).
  /// nullopt = size to content, clamped to window bounds.
  std::optional<Size> maxSize;

  /// Full-window dim behind the popover. Default is transparent (no dim).
  Color backdropColor = Color{0.f, 0.f, 0.f, 0.f};

  /// When set, the overlay anchor height is clamped to this value (use the trigger row height).
  std::optional<float> anchorMaxHeight;

  // ── Behaviour ────────────────────────────────────────────────────────────

  bool dismissOnEscape = true;
  bool dismissOnOutsideTap = true;

  /// Set when presenting via \ref usePopover (initial resolve) and on each overlay rebuild when
  /// \ref OverlayConfig::popoverPreferredPlacement is set.
  PopoverPlacement resolvedPlacement = placement;

  // ── Component protocol ─────────────────────────────────────────────────────

  /// body() wraps content in the card container. Not the primary API —
  /// use usePopover() instead.
  Element body() const;

  static constexpr float kArrowW = 10.f;
  static constexpr float kArrowH = 6.f;
};

/// Hook: returns (show, hide, isPresented) for presenting a Popover.
///
/// show(popover) — anchors to the tapped control's layout rect when opened from a pointer tap, otherwise
/// to the hook caller's composite layout rect (same component as `usePopover()`).
/// hide()        — dismisses the popover.
/// isPresented   — true while the popover is on screen.
///
/// Must be called inside body() like other hooks.
std::tuple<std::function<void(Popover)>, std::function<void()>, bool> usePopover();

/// Resolves preferred placement (flip above/below or start/end) from anchor and window size.
PopoverPlacement resolvePopoverPlacement(PopoverPlacement preferred, std::optional<Rect> const& anchor,
                                         std::optional<Size> const& maxSize, float gapTotal, Size win);

Vec2 popoverOverlayGapOffset(PopoverPlacement resolved, float gap);

OverlayConfig::Placement overlayPlacementFromPopover(PopoverPlacement p);

} // namespace flux
