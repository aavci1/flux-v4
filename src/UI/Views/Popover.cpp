#include <Flux/UI/Views/Popover.hpp>

#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/OverlaySurfaceHelpers.hpp>

#include <algorithm>

namespace flux {

namespace {

float availableAbove(Rect const& anchor) {
  return std::max(0.f, anchor.y);
}

float availableBelow(Rect const& anchor, Size window) {
  return std::max(0.f, window.height - (anchor.y + anchor.height));
}

float availableStart(Rect const& anchor) {
  return std::max(0.f, anchor.x);
}

float availableEnd(Rect const& anchor, Size window) {
  return std::max(0.f, window.width - (anchor.x + anchor.width));
}

} // namespace

Element Popover::body() const {
  Theme const& theme = useEnvironment<Theme>();
  ResolvedPopoverCardBody const surface =
      resolvePopoverCardBody(backgroundColor, borderColor, borderWidth, cornerRadius,
                             contentPadding, theme);

  Element body = content;
  return std::move(body)
      .padding(surface.contentPadding)
      .fill(surface.background)
      .stroke(StrokeStyle::solid(surface.border, surface.borderWidth))
      .cornerRadius(surface.cornerRadius);
}

std::tuple<std::function<void(Popover)>, std::function<void()>, bool> usePopover() {
  auto [showOverlay, hideOverlay, isPresented] = useOverlay();
  Runtime* runtime = Runtime::current();
  Window* window = runtime ? &runtime->window() : nullptr;

  auto show = [showOverlay = std::move(showOverlay), window](Popover popover) mutable {
    Theme const theme = window ? window->theme() : Theme::light();
    PopoverPlacement const preferred = popover.placement;
    std::optional<Rect> const anchor = popover.anchorRectOverride;
    Size const windowSize = window ? window->getSize() : Size{};
    PopoverPlacement const resolved =
        anchor ? resolvePopoverPlacement(preferred, anchor, popover.maxSize, popover.gap, windowSize)
               : preferred;
    popover.resolvedPlacement = resolved;

    OverlayConfig config{
        .anchor = anchor,
        .anchorMaxHeight = popover.anchorMaxHeight,
        .anchorOutsets = popover.anchorOutsets,
        .placement = overlayPlacementFromPopover(resolved),
        .crossAlignment = popover.crossAlignment,
        .offset = popoverOverlayGapOffset(resolved, popover.gap),
        .maxSize = popover.maxSize,
        .modal = false,
        .backdropColor = resolvePopoverBackdropColor(popover.backdropColor, theme),
        .dismissOnOutsideTap = popover.dismissOnOutsideTap,
        .dismissOnEscape = popover.dismissOnEscape,
        .debugName = popover.debugName,
    };

    showOverlay(Element{std::move(popover)}, std::move(config));
  };

  return {std::move(show), std::move(hideOverlay), isPresented};
}

PopoverPlacement resolvePopoverPlacement(PopoverPlacement preferred, std::optional<Rect> const& anchor,
                                         std::optional<Size> const& maxSize, float gapTotal,
                                         Size window) {
  if (!anchor) {
    return preferred;
  }
  Size const desired = maxSize.value_or(Size{0.f, 0.f});
  float const desiredWidth = desired.width + gapTotal;
  float const desiredHeight = desired.height + gapTotal;

  switch (preferred) {
  case PopoverPlacement::Below:
    if (desiredHeight > 0.f && availableBelow(*anchor, window) < desiredHeight &&
        availableAbove(*anchor) > availableBelow(*anchor, window)) {
      return PopoverPlacement::Above;
    }
    return PopoverPlacement::Below;
  case PopoverPlacement::Above:
    if (desiredHeight > 0.f && availableAbove(*anchor) < desiredHeight &&
        availableBelow(*anchor, window) > availableAbove(*anchor)) {
      return PopoverPlacement::Below;
    }
    return PopoverPlacement::Above;
  case PopoverPlacement::End:
    if (desiredWidth > 0.f && availableEnd(*anchor, window) < desiredWidth &&
        availableStart(*anchor) > availableEnd(*anchor, window)) {
      return PopoverPlacement::Start;
    }
    return PopoverPlacement::End;
  case PopoverPlacement::Start:
    if (desiredWidth > 0.f && availableStart(*anchor) < desiredWidth &&
        availableEnd(*anchor, window) > availableStart(*anchor)) {
      return PopoverPlacement::End;
    }
    return PopoverPlacement::Start;
  }
  return preferred;
}

PopoverPlacement resolveMeasuredPopoverPlacement(PopoverPlacement preferred,
                                                 std::optional<Rect> const& anchor,
                                                 Size popoverSize, float gap, Size window) {
  return resolvePopoverPlacement(preferred, anchor, popoverSize, gap, window);
}

Vec2 popoverOverlayGapOffset(PopoverPlacement resolved, float gap) {
  switch (resolved) {
  case PopoverPlacement::Below:
    return Vec2{0.f, gap};
  case PopoverPlacement::Above:
    return Vec2{0.f, -gap};
  case PopoverPlacement::End:
    return Vec2{gap, 0.f};
  case PopoverPlacement::Start:
    return Vec2{-gap, 0.f};
  }
  return Vec2{};
}

OverlayConfig::Placement overlayPlacementFromPopover(PopoverPlacement placement) {
  switch (placement) {
  case PopoverPlacement::Below:
    return OverlayConfig::Placement::Below;
  case PopoverPlacement::Above:
    return OverlayConfig::Placement::Above;
  case PopoverPlacement::End:
    return OverlayConfig::Placement::End;
  case PopoverPlacement::Start:
    return OverlayConfig::Placement::Start;
  }
  return OverlayConfig::Placement::Below;
}

} // namespace flux
