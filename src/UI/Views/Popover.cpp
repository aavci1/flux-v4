#include <Flux/UI/OverlaySurfaceHelpers.hpp>
#include <Flux/UI/Views/Popover.hpp>

#include <Flux/Core/Types.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace flux {

namespace {

Size estimateContentSize(std::optional<Size> const& maxSize) {
  float w = 200.f;
  float h = 200.f;
  if (maxSize.has_value()) {
    if (std::isfinite(maxSize->width) && maxSize->width > 0.f) {
      w = maxSize->width;
    }
    if (std::isfinite(maxSize->height) && maxSize->height > 0.f) {
      h = maxSize->height;
    }
  }
  return {w, h};
}

Rect applyAnchorOutsets(Rect rect, EdgeInsets const &outsets) {
  float const left = std::max(0.f, outsets.left);
  float const right = std::max(0.f, outsets.right);
  float const top = std::max(0.f, outsets.top);
  float const bottom = std::max(0.f, outsets.bottom);
  return Rect{
      rect.x - left,
      rect.y - top,
      rect.width + left + right,
      rect.height + top + bottom,
  };
}

char const *placementName(PopoverPlacement placement) {
  switch (placement) {
  case PopoverPlacement::Below:
    return "below";
  case PopoverPlacement::Above:
    return "above";
  case PopoverPlacement::End:
    return "end";
  case PopoverPlacement::Start:
    return "start";
  }
  return "unknown";
}

} // namespace

PopoverPlacement resolvePopoverPlacement(PopoverPlacement preferred, std::optional<Rect> const& anchor,
                                         std::optional<Size> const& maxSize, float gapTotal, Size win) {
  if (!anchor.has_value()) {
    return preferred;
  }
  Rect const& a = *anchor;
  Size const est = estimateContentSize(maxSize);

  switch (preferred) {
  case PopoverPlacement::Below: {
    float const bottom = a.y + a.height + gapTotal + est.height;
    if (bottom <= win.height) {
      return PopoverPlacement::Below;
    }
    return PopoverPlacement::Above;
  }
  case PopoverPlacement::Above: {
    float const top = a.y - gapTotal - est.height;
    if (top >= 0.f) {
      return PopoverPlacement::Above;
    }
    return PopoverPlacement::Below;
  }
  case PopoverPlacement::End: {
    float const right = a.x + a.width + gapTotal + est.width;
    if (right <= win.width) {
      return PopoverPlacement::End;
    }
    return PopoverPlacement::Start;
  }
  case PopoverPlacement::Start: {
    float const left = a.x - gapTotal - est.width;
    if (left >= 0.f) {
      return PopoverPlacement::Start;
    }
    return PopoverPlacement::End;
  }
  }
  return preferred;
}

Vec2 popoverOverlayGapOffset(PopoverPlacement resolved, float gap) {
  Vec2 offset{};
  switch (resolved) {
  case PopoverPlacement::Below:
    offset.y = gap;
    break;
  case PopoverPlacement::Above:
    offset.y = -gap;
    break;
  case PopoverPlacement::End:
    offset.x = gap;
    break;
  case PopoverPlacement::Start:
    offset.x = -gap;
    break;
  }
  return offset;
}

OverlayConfig::Placement overlayPlacementFromPopover(PopoverPlacement p) {
  switch (p) {
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

Element Popover::body() const {
  Theme const& theme = useEnvironment<Theme>();
  ResolvedPopoverCardBody const card =
      resolvePopoverCardBody(backgroundColor, borderColor, borderWidth, cornerRadius, contentPadding, theme);
  return Element{PopoverCalloutShape{
      .placement = resolvedPlacement,
      .arrow = arrow,
      .padding = card.contentPadding,
      .cornerRadius = card.cornerRadius,
      .backgroundColor = card.background,
      .borderColor = card.border,
      .borderWidth = card.borderWidth,
      .maxSize = maxSize,
      .content = content,
  }};
}

std::tuple<std::function<void(Popover)>, std::function<void()>, bool> usePopover() {
  StateStore* store = StateStore::current();
  Runtime* rt = Runtime::current();
  assert(store && "usePopover called outside of a build pass");
  assert(rt && "usePopover called outside of a build pass");

  auto [showOverlay, hideOverlay, isPresented] = useOverlay();
  ComponentKey const anchorKey = store->currentComponentKey();
  Window* wPtr = &rt->window();

  auto show = [showOverlay, hideOverlay, anchorKey, rt, wPtr, store](Popover popover) {
    std::optional<Rect> anchorRect;
    std::optional<ComponentKey> anchorTrackLeafKey;
    std::optional<ComponentKey> anchorTrackComponentKey;
    char const *anchorSource = "none";
    if (popover.anchorRectOverride.has_value()) {
      anchorRect = popover.anchorRectOverride;
      anchorTrackLeafKey = std::nullopt;
      anchorSource = "override";
    } else if (popover.useHoverLeafAnchor) {
      // Same anchor resolution as tap-driven popovers (see popover-demo onTap): forLeafKeyPrefix on a
      // leaf key, plus tracking key for scroll. Uses hover leaf while the tooltip trigger is hovered.
      ComponentKey const hk = rt->hover().hoveredKey();
      if (!hk.empty() && rt->hover().isInSubtree(anchorKey, *store)) {
        anchorRect = rt->layoutRectForLeafKeyPrefix(hk);
        anchorTrackLeafKey = hk;
        anchorSource = "hover-leaf";
      }
    } else if (popover.useTapAnchor) {
      anchorRect = rt->layoutRectForTapAnchor();
      anchorTrackLeafKey = rt->tapAnchorLeafKeySnapshot();
      anchorSource = "tap-leaf";
    } else {
      anchorRect = rt->layoutRectForKey(anchorKey);
      anchorTrackComponentKey = anchorKey;
      anchorSource = "component";
    }
    if (!anchorRect.has_value()) {
      anchorRect = rt->layoutRectForKey(anchorKey);
      anchorTrackLeafKey = std::nullopt;
      anchorTrackComponentKey = anchorKey;
      anchorSource = "component-fallback";
    }
    // Exact composite key is sometimes absent from the cache during the same pass; walk prefixes so we
    // still anchor to an ancestor composite rect (same idea as tap tracking).
    if (!anchorRect.has_value()) {
      anchorRect = rt->layoutRectForLeafKeyPrefix(anchorKey);
      anchorTrackLeafKey = std::nullopt;
      anchorTrackComponentKey = anchorKey;
      anchorSource = "prefix-fallback";
    }
    if (anchorRect.has_value() && popover.anchorMaxHeight.has_value()) {
      anchorRect->height = std::min(anchorRect->height, *popover.anchorMaxHeight);
    }
    if (anchorRect.has_value() && !popover.anchorOutsets.isZero()) {
      anchorRect = applyAnchorOutsets(*anchorRect, popover.anchorOutsets);
    }
    Size const win = wPtr->getSize();
    Theme const* tp = wPtr->environmentValue<Theme>();
    Theme const theme = tp ? *tp : Theme::light();
    float const gap = resolveFloat(popover.gap, theme.space2);
    // Space needed for flip heuristic: gap + arrow height (arrow is inside overlay bounds, not in offset).
    float const gapTotal = gap + (popover.arrow ? Popover::kArrowH : 0.f);
    std::optional<Size> const maxSz = popover.maxSize;
    bool const dismissOutside = popover.dismissOnOutsideTap;
    bool const dismissEsc = popover.dismissOnEscape;

    PopoverPlacement const preferred = popover.placement;
    PopoverPlacement const resolved =
        resolvePopoverPlacement(preferred, anchorRect, maxSz, gapTotal, win);
    popover.resolvedPlacement = resolved;
    std::string const debugName = popover.debugName;

    Vec2 const offset = popoverOverlayGapOffset(resolved, gap);

    Color const backdropResolved = resolvePopoverBackdropColor(popover.backdropColor, theme);

    if (!debugName.empty()) {
      if (anchorRect.has_value()) {
        std::fprintf(stderr,
                     "[popover:%s] show source=%s anchor=(%.1f, %.1f, %.1f, %.1f) preferred=%s resolved=%s gap=%.1f keyDepth=%zu\n",
                     debugName.c_str(), anchorSource, anchorRect->x, anchorRect->y,
                     anchorRect->width, anchorRect->height, placementName(preferred),
                     placementName(resolved), gap, anchorKey.size());
      } else {
        std::fprintf(stderr,
                     "[popover:%s] show source=%s anchor=(none) preferred=%s resolved=%s gap=%.1f keyDepth=%zu\n",
                     debugName.c_str(), anchorSource, placementName(preferred),
                     placementName(resolved), gap, anchorKey.size());
      }
    }

    showOverlay(
        Element{std::move(popover)},
        OverlayConfig{
            .anchor = anchorRect,
            .anchorTrackLeafKey = std::move(anchorTrackLeafKey),
            .anchorTrackComponentKey = std::move(anchorTrackComponentKey),
            .anchorMaxHeight = popover.anchorMaxHeight,
            .anchorOutsets = popover.anchorOutsets,
            .placement = overlayPlacementFromPopover(resolved),
            .crossAlignment = popover.crossAlignment,
            .offset = offset,
            .maxSize = maxSz,
            .modal = false,
            .backdropColor = backdropResolved,
            .popoverPreferredPlacement = preferred,
            .popoverGapTotal = gapTotal,
            .popoverGap = gap,
            .dismissOnOutsideTap = dismissOutside,
            .dismissOnEscape = dismissEsc,
            .onDismiss = hideOverlay,
            .debugName = debugName,
        });
  };

  return {std::move(show), hideOverlay, isPresented};
}

} // namespace flux
