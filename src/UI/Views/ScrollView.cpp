#include <Flux/UI/Element.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/UI/Detail/InvalidationBridge.hpp>
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

bool incrementalScrollEnabled() {
    Runtime *runtime = Runtime::current();
    StateStore *store = StateStore::current();
    return runtime && runtime->incrementalUpdatesEnabled() && store && !store->overlayScope().has_value();
}

[[nodiscard]] Point maxScrollOffset(ScrollAxis axis, Size const &viewport, Size const &content);
[[nodiscard]] ScrollIndicatorMetrics makeVerticalIndicator(Point const &offset, Size const &viewport, Size const &content,
                                                           bool reserveBottom);
[[nodiscard]] ScrollIndicatorMetrics makeHorizontalIndicator(Point const &offset, Size const &viewport, Size const &content,
                                                             bool reserveTrailing);

struct SceneRectBinding {
    Runtime *runtime = nullptr;
    NodeId nodeId {};

    void bind(NodeId id, Rect rect, Color color, float opacity) {
        nodeId = id;
        update(rect, color, opacity);
    }

    void update(Rect rect, Color color, float opacity) const {
        if (!runtime || !nodeId.isValid()) {
            return;
        }
        if (RectNode *node = runtime->window().sceneGraph().node<RectNode>(nodeId)) {
            color.a *= opacity;
            node->bounds = rect;
            node->fill = FillStyle::solid(color);
        }
    }
};

struct BoundIndicatorRect {
    std::shared_ptr<SceneRectBinding> binding;
    Rect rect {};
    Color color {};
    float opacity = 1.f;
    CornerRadius radius {};

    void layout(LayoutContext &ctx) const {
        ComponentKey const stableKey = ctx.leafComponentKey();
        ctx.advanceChildSlot();
        LayoutNode n {};
        n.kind = LayoutNode::Kind::Leaf;
        n.frame = rect;
        n.componentKey = stableKey;
        n.element = ctx.currentElement();
        n.constraints = ctx.constraints();
        n.hints = ctx.hints();
        ctx.pushLayoutNode(std::move(n));
    }

    void renderFromLayout(RenderContext &ctx, LayoutNode const &node) const {
        NodeId const id = ctx.graph().addRect(ctx.parentLayer(), RectNode {
                                                     .bounds = node.frame,
                                                     .cornerRadius = radius,
                                                     .fill = FillStyle::solid(Color {color.r, color.g, color.b, color.a * opacity}),
                                                     .stroke = StrokeStyle::none(),
                                                 });
        if (binding) {
            binding->bind(id, node.frame, color, opacity);
        }
    }

    Size measure(LayoutContext &ctx, LayoutConstraints const &, LayoutHints const &, TextSystem &) const {
        ctx.advanceChildSlot();
        return {rect.width, rect.height};
    }
};

struct ScrollRuntimeState {
    Runtime *runtime = nullptr;
    ComponentKey key {};
    ScrollAxis axis = ScrollAxis::Vertical;
    Point offset {};
    Point lastAppliedOffset {};
    Point downPoint {};
    bool dragging = false;
    Size viewport {};
    Size content {};
    float indicatorOpacity = 0.f;
    Color indicatorColor {};
    std::shared_ptr<SceneRectBinding> verticalBinding = std::make_shared<SceneRectBinding>();
    std::shared_ptr<SceneRectBinding> horizontalBinding = std::make_shared<SceneRectBinding>();

    void attach(Runtime *rt, ComponentKey const &componentKey, ScrollAxis nextAxis, Size nextViewport, Size nextContent) {
        runtime = rt;
        key = componentKey;
        axis = nextAxis;
        viewport = nextViewport;
        content = nextContent;
        verticalBinding->runtime = rt;
        horizontalBinding->runtime = rt;
        offset = clampScrollOffset(axis, offset, viewport, content);
        lastAppliedOffset = offset;
    }

    Point clampedOffset() const {
        return clampScrollOffset(axis, offset, viewport, content);
    }

    void applyIndicators(Color indicatorColor) const {
        if (verticalBinding) {
            ScrollIndicatorMetrics const vertical =
                makeVerticalIndicator(clampedOffset(), viewport, content, maxScrollOffset(ScrollAxis::Horizontal, viewport, content).x > 0.f);
            verticalBinding->update(
                Rect {vertical.x, vertical.y, vertical.width, vertical.height},
                indicatorColor,
                indicatorOpacity
            );
        }
        if (horizontalBinding) {
            ScrollIndicatorMetrics const horizontal =
                makeHorizontalIndicator(clampedOffset(), viewport, content, maxScrollOffset(ScrollAxis::Vertical, viewport, content).y > 0.f);
            horizontalBinding->update(
                Rect {horizontal.x, horizontal.y, horizontal.width, horizontal.height},
                indicatorColor,
                indicatorOpacity
            );
        }
    }

    void applyScene(Color indicatorColor) {
        this->indicatorColor = indicatorColor;
        if (!runtime) {
            return;
        }
        Point const clamped = clampedOffset();
        Vec2 const delta {
            lastAppliedOffset.x - clamped.x,
            lastAppliedOffset.y - clamped.y,
        };
        if (std::optional<NodeId> layerId = runtime->sceneLayerForComponentKey(key)) {
            if (LayerNode *layer = runtime->window().sceneGraph().node<LayerNode>(*layerId)) {
                float const baseX = layer->transform.m[6] + lastAppliedOffset.x;
                float const baseY = layer->transform.m[7] + lastAppliedOffset.y;
                layer->transform = Mat3::translate(baseX - clamped.x, baseY - clamped.y);
            }
        }
        runtime->layoutRects().translateSubtree(key, delta);
        lastAppliedOffset = clamped;
        applyIndicators(indicatorColor);
        runtime->rebuildOverlays();
        runtime->invalidateSubtree(key, InvalidationKind::Transform);
    }
};

struct ScrollLocalState {
    Signal<Point> offset {Point {0.f, 0.f}};
    Signal<Size> viewport {Size {0.f, 0.f}};
    Signal<Size> content {Size {0.f, 0.f}};
    Animated<float> indicatorOpacity {0.f};
    Point downPoint {};
    bool dragging = false;
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

[[nodiscard]] Element makeBoundIndicatorOverlay(ScrollRuntimeState const &state,
                                                ScrollIndicatorMetrics const &verticalIndicator,
                                                ScrollIndicatorMetrics const &horizontalIndicator,
                                                Color const &indicatorColor) {
    std::vector<Element> indicatorChildren;
    indicatorChildren.reserve(2);

    if (verticalIndicator.visible()) {
        indicatorChildren.emplace_back(
            BoundIndicatorRect {
                .binding = state.verticalBinding,
                .rect = Rect {verticalIndicator.x, verticalIndicator.y, verticalIndicator.width, verticalIndicator.height},
                .color = indicatorColor,
                .opacity = state.indicatorOpacity,
                .radius = CornerRadius {verticalIndicator.width * 0.5f},
            }
        );
    }
    if (horizontalIndicator.visible()) {
        indicatorChildren.emplace_back(
            BoundIndicatorRect {
                .binding = state.horizontalBinding,
                .rect = Rect {horizontalIndicator.x, horizontalIndicator.y, horizontalIndicator.width, horizontalIndicator.height},
                .color = indicatorColor,
                .opacity = state.indicatorOpacity,
                .radius = CornerRadius {horizontalIndicator.height * 0.5f},
            }
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
    bool const fastPath = incrementalScrollEnabled();
    Rect const bounds = useBounds();
    StateStore *store = StateStore::current();
    ScrollLocalState *localState = store ? &store->claimSlot<ScrollLocalState>() : nullptr;
    Runtime *runtime = Runtime::current();
    if (localState && store) {
        ComponentKey const key = store->currentComponentKey();
        localState->offset.setInvalidationCallback(detail::makeInvalidationCallback(runtime, key, InvalidationKind::Build));
        localState->viewport.setInvalidationCallback(detail::makeInvalidationCallback(runtime, key, InvalidationKind::Build));
        localState->content.setInvalidationCallback(detail::makeInvalidationCallback(runtime, key, InvalidationKind::Build));
        localState->indicatorOpacity.setInvalidationCallback(detail::makeInvalidationCallback(runtime, key, InvalidationKind::Build));
    }
    State<Point> const offset = fastPath ? State<Point> {} :
                                     (scrollOffset.signal ? scrollOffset :
                                                            State<Point> {localState ? &localState->offset : nullptr});
    State<Size> const viewport = viewportSize.signal ? viewportSize :
                                 State<Size> {localState ? &localState->viewport : nullptr};
    State<Size> const content = contentSize.signal ? contentSize :
                                State<Size> {localState ? &localState->content : nullptr};
    Anim<float> const indicatorOpacity =
        localState ? Anim<float> {&localState->indicatorOpacity} : useAnimated<float>(0.f);
    ScrollRuntimeState *scrollState = fastPath && store ? &store->claimSlot<ScrollRuntimeState>() : nullptr;
    if (scrollState && store) {
        Size const viewportSeed = resolveViewportSize(viewport.signal ? *viewport : Size {}, bounds);
        Size const contentSeed = content.signal ? *content : Size {};
        scrollState->attach(runtime, store->currentComponentKey(), axis, viewportSeed, contentSeed);
        scrollState->indicatorColor = indicatorColorForTheme(theme);
    }
    Size const effectiveViewport = resolveViewportSize(*viewport, bounds);
    ScrollAxis const ax = axis;
    bool const dragScroll = dragScrollEnabled;
    Size const contentSize = *content;
    Point const scrollRange = maxScrollOffset(ax, effectiveViewport, contentSize);
    Point const clampedOffset =
        fastPath && scrollState ? scrollState->clampedOffset() : clampScrollOffset(ax, *offset, effectiveViewport, contentSize);
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
    if (fastPath && scrollState) {
        scrollState->viewport = effectiveViewport;
        scrollState->content = contentSize;
        scrollState->indicatorOpacity = (showsVerticalIndicator || showsHorizontalIndicator) ? 1.f : 0.f;
        scrollContent = std::move(scrollContent).overlay(
            makeBoundIndicatorOverlay(*scrollState, verticalIndicator, horizontalIndicator, indicatorColor)
        );
    } else {
        scrollContent = std::move(scrollContent).overlay(
            makeIndicatorOverlay(verticalIndicator, horizontalIndicator, indicatorColor, *indicatorOpacity)
        );
    }
    auto revealIndicators = [indicatorOpacity, indicatorShow, indicatorHide, fastPath, scrollState]() {
        if (fastPath && scrollState) {
            scrollState->indicatorOpacity = 1.f;
            return;
        }
        indicatorOpacity.set(1.f, indicatorShow);
        indicatorOpacity.set(0.f, indicatorHide);
    };

    return std::move(scrollContent)
        .clipContent(true)
        .onPointerDown(
            [scrollState, localState, dragScroll, clampedOffset](Point p) {
                if (!dragScroll) {
                    return;
                }
                if (scrollState) {
                    scrollState->dragging = true;
                    scrollState->downPoint = Point {p.x + clampedOffset.x, p.y + clampedOffset.y};
                    return;
                }
                if (localState) {
                    localState->dragging = true;
                    localState->downPoint = Point {p.x + clampedOffset.x, p.y + clampedOffset.y};
                }
            }
        )
        .onPointerUp(
            [scrollState, localState, dragScroll](Point) {
                if (!dragScroll) {
                    return;
                }
                if (scrollState) {
                    scrollState->dragging = false;
                    return;
                }
                if (localState) {
                    localState->dragging = false;
                }
            }
        )
        .onPointerMove(
            [offset, scrollState, localState, ax, content, effectiveViewport, revealIndicators, dragScroll, indicatorColor](Point p) {
                if (!dragScroll) {
                    return;
                }
                if (scrollState && !scrollState->dragging) {
                    return;
                }
                if (!scrollState && (!localState || !localState->dragging)) {
                    return;
                }
                Point const next = scrollState ? Point {scrollState->downPoint.x - p.x, scrollState->downPoint.y - p.y}
                                               : Point {localState->downPoint.x - p.x, localState->downPoint.y - p.y};
                if (scrollState) {
                    scrollState->offset = clampScrollOffset(ax, next, effectiveViewport, *content);
                    revealIndicators();
                    scrollState->applyScene(indicatorColor);
                    return;
                }
                offset = clampScrollOffset(ax, next, effectiveViewport, *content);
                revealIndicators();
            }
        )
        .onScroll(
            [offset, scrollState, ax, content, effectiveViewport, revealIndicators, indicatorColor](Vec2 d) {
                Point next = scrollState ? scrollState->offset : *offset;
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
                if (scrollState) {
                    scrollState->offset = clamped;
                    revealIndicators();
                    scrollState->applyScene(indicatorColor);
                    return;
                }
                offset = clamped;
                revealIndicators();
            }
        );
}

} // namespace flux
