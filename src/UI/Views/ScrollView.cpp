#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/ScrollView.hpp>

#include <Flux/Reactive/Transition.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace flux {

namespace {

struct ScrollIndicatorMetrics {
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;

    [[nodiscard]] bool visible() const { return width > 0.f && height > 0.f; }
};

struct ScrollIndicatorStyle {
    static constexpr float thickness = 4.f;
    static constexpr float outerInset = 3.f;
    static constexpr float minLength = 24.f;
};

[[nodiscard]] float indicatorTrackLength(float viewportExtent, bool reserveTrailing) {
    float const trailingInset =
        ScrollIndicatorStyle::outerInset +
        (reserveTrailing ? ScrollIndicatorStyle::thickness + ScrollIndicatorStyle::outerInset : 0.f);
    return std::max(0.f, viewportExtent - ScrollIndicatorStyle::outerInset - trailingInset);
}

[[nodiscard]] float indicatorThumbLength(float viewportExtent, float contentExtent, float trackLength) {
    return std::clamp((viewportExtent / contentExtent) * trackLength, ScrollIndicatorStyle::minLength, trackLength);
}

[[nodiscard]] Point maxScrollOffset(ScrollAxis axis, Size const &viewport, Size const &content) {
    return Point {
        (axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both) ? std::max(0.f, content.width - viewport.width) : 0.f,
        (axis == ScrollAxis::Vertical || axis == ScrollAxis::Both) ? std::max(0.f, content.height - viewport.height) : 0.f,
    };
}

[[nodiscard]] Size resolveViewportSize(Size viewport, std::optional<Rect> const &layoutRect) {
    if (viewport.width <= 0.f && layoutRect && layoutRect->width > 0.f) {
        viewport.width = layoutRect->width;
    }
    if (viewport.height <= 0.f && layoutRect && layoutRect->height > 0.f) {
        viewport.height = layoutRect->height;
    }
    return viewport;
}

[[nodiscard]] Color indicatorColorForTheme(Theme const &theme) {
    return Color {
        theme.colorTextSecondary.r,
        theme.colorTextSecondary.g,
        theme.colorTextSecondary.b,
        0.55f,
    };
}

[[nodiscard]] Element makeIndicatorOverlay(ScrollIndicatorMetrics const &verticalIndicator,
                                           ScrollIndicatorMetrics const &horizontalIndicator,
                                           Color const &indicatorColor,
                                           float indicatorOpacity) {
    std::vector<Element> indicatorChildren;
    indicatorChildren.reserve(2);

    if (verticalIndicator.visible()) {
        indicatorChildren.emplace_back(
            Rectangle {}
                .fill(FillStyle::solid(indicatorColor))
                .position(verticalIndicator.x, verticalIndicator.y)
                .size(verticalIndicator.width, verticalIndicator.height)
                .cornerRadius(CornerRadius {verticalIndicator.width * 0.5f})
                .opacity(indicatorOpacity)
        );
    }
    if (horizontalIndicator.visible()) {
        indicatorChildren.emplace_back(
            Rectangle {}
                .fill(FillStyle::solid(indicatorColor))
                .position(horizontalIndicator.x, horizontalIndicator.y)
                .size(horizontalIndicator.width, horizontalIndicator.height)
                .cornerRadius(CornerRadius {horizontalIndicator.height * 0.5f})
                .opacity(indicatorOpacity)
        );
    }

    return Element {ZStack {
        .horizontalAlignment = Alignment::Start,
        .verticalAlignment = Alignment::Start,
        .children = std::move(indicatorChildren),
    }};
}

[[nodiscard]] ScrollIndicatorMetrics makeVerticalIndicator(Point const &offset, Size const &viewport, Size const &content,
                                                           bool reserveBottom) {
    if (viewport.width <= 0.f || viewport.height <= 0.f || content.height <= viewport.height) {
        return {};
    }

    float const trackLength = indicatorTrackLength(viewport.height, reserveBottom);
    if (trackLength <= 0.f) {
        return {};
    }

    float const maxOffset = maxScrollOffset(ScrollAxis::Vertical, viewport, content).y;
    float const thumbLength = indicatorThumbLength(viewport.height, content.height, trackLength);
    float const travel = std::max(0.f, trackLength - thumbLength);
    float const t = maxOffset > 0.f ? std::clamp(offset.y / maxOffset, 0.f, 1.f) : 0.f;

    return ScrollIndicatorMetrics {
        .x = std::max(0.f, viewport.width - ScrollIndicatorStyle::thickness - ScrollIndicatorStyle::outerInset),
        .y = ScrollIndicatorStyle::outerInset + travel * t,
        .width = ScrollIndicatorStyle::thickness,
        .height = thumbLength,
    };
}

[[nodiscard]] ScrollIndicatorMetrics makeHorizontalIndicator(Point const &offset, Size const &viewport, Size const &content,
                                                             bool reserveTrailing) {
    if (viewport.width <= 0.f || viewport.height <= 0.f || content.width <= viewport.width) {
        return {};
    }

    float const trackLength = indicatorTrackLength(viewport.width, reserveTrailing);
    if (trackLength <= 0.f) {
        return {};
    }

    float const maxOffset = maxScrollOffset(ScrollAxis::Horizontal, viewport, content).x;
    float const thumbLength = indicatorThumbLength(viewport.width, content.width, trackLength);
    float const travel = std::max(0.f, trackLength - thumbLength);
    float const t = maxOffset > 0.f ? std::clamp(offset.x / maxOffset, 0.f, 1.f) : 0.f;

    return ScrollIndicatorMetrics {
        .x = ScrollIndicatorStyle::outerInset + travel * t,
        .y = std::max(0.f, viewport.height - ScrollIndicatorStyle::thickness - ScrollIndicatorStyle::outerInset),
        .width = thumbLength,
        .height = ScrollIndicatorStyle::thickness,
    };
}

} // namespace

Point clampScrollOffset(ScrollAxis axis, Point o, Size const &viewport, Size const &content) {
    Point const maxOffset = maxScrollOffset(axis, viewport, content);
    Point r = o;
    if (axis == ScrollAxis::Vertical || axis == ScrollAxis::Both) {
        r.y = std::clamp(r.y, 0.f, maxOffset.y);
    } else {
        r.y = 0.f;
    }
    if (axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both) {
        r.x = std::clamp(r.x, 0.f, maxOffset.x);
    } else {
        r.x = 0.f;
    }
    return r;
}

Element ScrollView::body() const {
    Theme const &theme = useEnvironment<Theme>();
    State<Point> const offset = scrollOffset.signal ? scrollOffset : useState<Point>({0.f, 0.f});
    auto downPoint = useState<Point>({0.f, 0.f});
    auto dragging = useState(false);
    State<Size> const viewport = viewportSize.signal ? viewportSize : useState<Size>({0.f, 0.f});
    State<Size> const content = contentSize.signal ? contentSize : useState<Size>({0.f, 0.f});
    auto indicatorOpacity = useAnimated<float>(0.f);
    std::optional<Rect> const layoutRect = useLayoutRect();
    Size const effectiveViewport = resolveViewportSize(*viewport, layoutRect);
    ScrollAxis const ax = axis;
    bool const dragScroll = dragScrollEnabled;
    Size const contentSize = *content;
    Point const scrollRange = maxScrollOffset(ax, effectiveViewport, contentSize);
    Point const clampedOffset = clampScrollOffset(ax, *offset, effectiveViewport, contentSize);
    if (clampedOffset != *offset) {
        offset = clampedOffset;
    }
    bool const showsVerticalIndicator = scrollRange.y > 0.f;
    bool const showsHorizontalIndicator = scrollRange.x > 0.f;
    ScrollIndicatorMetrics const verticalIndicator =
        makeVerticalIndicator(clampedOffset, effectiveViewport, contentSize, showsHorizontalIndicator);
    ScrollIndicatorMetrics const horizontalIndicator =
        makeHorizontalIndicator(clampedOffset, effectiveViewport, contentSize, showsVerticalIndicator);
    Transition const indicatorShow = Transition::instant();
    Transition indicatorHide = Transition::linear(theme.reducedMotion ? 0.01f : theme.durationMedium);
    indicatorHide.delay = 0.85f;
    Color const indicatorColor = indicatorColorForTheme(theme);
    Element scrollContent = OffsetView {
        .offset = clampedOffset,
        .axis = ax,
        .viewportSize = viewport,
        .contentSize = content,
        .children = children,
    };
    scrollContent = std::move(scrollContent).overlay(makeIndicatorOverlay(verticalIndicator, horizontalIndicator, indicatorColor, *indicatorOpacity));
    auto revealIndicators = [indicatorOpacity, indicatorShow, indicatorHide]() {
        indicatorOpacity.set(1.f, indicatorShow);
        indicatorOpacity.set(0.f, indicatorHide);
    };

    return std::move(scrollContent)
        .clipContent(true)
        .onPointerDown(
            [dragging, downPoint, clampedOffset, dragScroll](Point p) {
                if (!dragScroll) {
                    return;
                }
                dragging = true;
                downPoint = Point {p.x + clampedOffset.x, p.y + clampedOffset.y};
            }
        )
        .onPointerUp(
            [dragging, dragScroll](Point) {
                if (!dragScroll) {
                    return;
                }
                dragging = false;
            }
        )
        .onPointerMove(
            [offset, downPoint, ax, content, dragging, effectiveViewport, revealIndicators, dragScroll](Point p) {
                if (!dragScroll) {
                    return;
                }
                if (!*dragging) {
                    return;
                }
                Point const next {(*downPoint).x - p.x, (*downPoint).y - p.y};
                offset = clampScrollOffset(ax, next, effectiveViewport, *content);
                revealIndicators();
            }
        )
        .onScroll(
            [offset, ax, content, effectiveViewport, revealIndicators](Vec2 d) {
                Point next = *offset;
                // scrollingDelta* is expressed for non-flipped NSView coords (y up). Flux uses
                // a flipped space (y down), so negate to align. Natural Scrolling is already in
                // the delta sign from AppKit; this is only the coordinate-system fix.
                if (ax == ScrollAxis::Vertical || ax == ScrollAxis::Both) {
                    next.y -= d.y;
                }
                if (ax == ScrollAxis::Horizontal || ax == ScrollAxis::Both) {
                    next.x -= d.x;
                }
                Point const clamped =
                    clampScrollOffset(ax, next, effectiveViewport, *content);
                offset = clamped;
                revealIndicators();
            }
        );
}

} // namespace flux
