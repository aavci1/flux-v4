#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/ScrollView.hpp>

#include <Flux/Detail/Runtime.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Reactive/Transition.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
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

struct ScrollSceneTarget {
    NodeId layerId{};
    Rect frame{};
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

bool mat3Equal(Mat3 const &a, Mat3 const &b) {
    for (int i = 0; i < 9; ++i) {
        if (a.m[i] != b.m[i]) {
            return false;
        }
    }
    return true;
}

std::optional<ScrollSceneTarget> findScrollSceneTarget(LayoutTree const &tree, LayoutNodeId id) {
    LayoutNode const *node = tree.get(id);
    if (!node) {
        return std::nullopt;
    }
    if (node->kind == LayoutNode::Kind::Container &&
        node->containerSpec.kind == ContainerLayerSpec::Kind::OffsetScroll &&
        node->sceneNodes.size() == 1) {
        return ScrollSceneTarget{
            .layerId = node->sceneNodes[0],
            .frame = node->frame,
        };
    }
    for (LayoutNodeId childId : node->children) {
        if (std::optional<ScrollSceneTarget> const target = findScrollSceneTarget(tree, childId)) {
            return target;
        }
    }
    return std::nullopt;
}

bool applyDirectScroll(Runtime *runtime, ComponentKey const &key, Point offset) {
    if (!runtime || runtime->window().overlayManager().hasOverlays()) {
        return false;
    }

    LayoutNode const *root = runtime->mainLayoutTree().nodeForKey(key);
    if (!root) {
        return false;
    }
    std::optional<ScrollSceneTarget> const target = findScrollSceneTarget(runtime->mainLayoutTree(), root->id);
    if (!target.has_value()) {
        return false;
    }

    LayerNode *layer = runtime->window().sceneGraph().node<LayerNode>(target->layerId);
    if (!layer) {
        return false;
    }

    Mat3 const next = Mat3::translate(target->frame.x - offset.x, target->frame.y - offset.y);
    if (mat3Equal(layer->transform, next)) {
        return true;
    }

    layer->transform = next;
    runtime->window().requestRedraw();
    return true;
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
    Runtime *runtime = Runtime::current();
    StateStore *store = StateStore::current();
    ComponentKey const componentKey = store ? store->currentComponentKey() : ComponentKey{};
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
    Point const scrollRange = maxScrollOffset(ax, effectiveViewport, contentSize);
    Point const clampedOffset = clampScrollOffset(ax, *offset, effectiveViewport, contentSize);
    bool const showsVerticalIndicator = scrollRange.y > 0.f;
    bool const showsHorizontalIndicator = scrollRange.x > 0.f;
    ScrollIndicatorMetrics const verticalIndicator =
        makeVerticalIndicator(clampedOffset, effectiveViewport, contentSize, showsHorizontalIndicator);
    ScrollIndicatorMetrics const horizontalIndicator =
        makeHorizontalIndicator(clampedOffset, effectiveViewport, contentSize, showsVerticalIndicator);
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
    auto commitOffset = [offset, runtime, componentKey](Point clamped) {
        if (runtime && applyDirectScroll(runtime, componentKey, clamped)) {
            offset.setSilently(clamped);
            return true;
        }
        offset = clamped;
        return false;
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
                if (!commitOffset(clamped)) {
                    revealIndicators();
                }
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
                if (!commitOffset(clamped)) {
                    revealIndicators();
                }
            }
        );
}

} // namespace flux
