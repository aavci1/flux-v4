#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/ScrollView.hpp>

#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Reactive/Transition.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "UI/Layout/Algorithms/ScrollLayout.hpp"

namespace flux {

namespace {

[[nodiscard]] Size resolveViewportSize(Size viewport, Rect const &bounds) {
    if (viewport.width <= 0.f && bounds.width > 0.f) {
        viewport.width = bounds.width;
    }
    if (viewport.height <= 0.f && bounds.height > 0.f) {
        viewport.height = bounds.height;
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

[[nodiscard]] Element makeIndicatorOverlay(layout::ScrollIndicatorMetrics const &verticalIndicator,
                                           layout::ScrollIndicatorMetrics const &horizontalIndicator,
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

} // namespace

Point clampScrollOffset(ScrollAxis axis, Point o, Size const &viewport, Size const &content) {
    return layout::clampScrollOffset(axis, o, viewport, content);
}

Element ScrollView::body() const {
    Theme const &theme = useEnvironment<Theme>();
    State<Point> const offset = scrollOffset.signal ? scrollOffset : useState<Point>({0.f, 0.f});
    auto downPoint = useState<Point>({0.f, 0.f});
    auto dragging = useState(false);
    State<Size> const viewport = viewportSize.signal ? viewportSize : useState<Size>({0.f, 0.f});
    State<Size> const content = contentSize.signal ? contentSize : useState<Size>({0.f, 0.f});
    auto indicatorOpacity = useAnimation<float>(0.f);
    Rect const bounds = useBounds();
    Size const effectiveViewport = resolveViewportSize(*viewport, bounds);
    ScrollAxis const ax = axis;
    bool const dragScroll = dragScrollEnabled;
    Size const contentSize = *content;
    Point const scrollRange = layout::maxScrollOffset(ax, effectiveViewport, contentSize);
    Point const clampedOffset = clampScrollOffset(ax, *offset, effectiveViewport, contentSize);
    bool const showsVerticalIndicator = scrollRange.y > 0.f;
    bool const showsHorizontalIndicator = scrollRange.x > 0.f;
    layout::ScrollIndicatorMetrics const verticalIndicator =
        layout::makeVerticalIndicator(clampedOffset, effectiveViewport, contentSize, showsHorizontalIndicator);
    layout::ScrollIndicatorMetrics const horizontalIndicator =
        layout::makeHorizontalIndicator(clampedOffset, effectiveViewport, contentSize, showsVerticalIndicator);
    bool const showsAnyIndicator = verticalIndicator.visible() || horizontalIndicator.visible();
    Transition const indicatorShow = Transition::instant();
    Transition const indicatorHide = Transition::linear(theme.durationMedium).delayed(0.85f);
    Color const indicatorColor = indicatorColorForTheme(theme);
    Element scrollContent = OffsetView {
        .offset = clampedOffset,
        .axis = ax,
        .viewportSize = viewport,
        .contentSize = content,
        .children = children,
    };
    std::vector<Element> layers;
    layers.reserve(2);
    layers.push_back(std::move(scrollContent));
    if (showsAnyIndicator) {
        layers.push_back(makeIndicatorOverlay(verticalIndicator, horizontalIndicator, indicatorColor, *indicatorOpacity));
    }
    Element root = Element {ZStack {
        .horizontalAlignment = Alignment::Start,
        .verticalAlignment = Alignment::Start,
        .children = std::move(layers),
    }};
    auto revealIndicators = [indicatorOpacity, indicatorShow, indicatorHide, showsAnyIndicator]() {
        if (!showsAnyIndicator) {
            return;
        }
        indicatorOpacity.set(1.f, indicatorShow);
        indicatorOpacity.set(0.f, indicatorHide);
    };
    auto commitOffset = [offset](Point clamped) {
        offset = clamped;
        return true;
    };

    return std::move(root)
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
            [commitOffset, downPoint, ax, content, dragging, effectiveViewport, revealIndicators, dragScroll](Point p) {
                if (!dragScroll) {
                    return;
                }
                if (!*dragging) {
                    return;
                }
                Point const next {(*downPoint).x - p.x, (*downPoint).y - p.y};
                Point const clamped = clampScrollOffset(ax, next, effectiveViewport, *content);
                commitOffset(clamped);
                revealIndicators();
            }
        )
        .onScroll(
            [offset, commitOffset, ax, content, effectiveViewport, revealIndicators](Vec2 d) {
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
                commitOffset(clamped);
                revealIndicators();
            }
        );
}

} // namespace flux
