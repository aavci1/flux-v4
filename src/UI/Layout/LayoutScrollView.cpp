#include <Flux/UI/Element.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cmath>

namespace flux {

namespace {

Size resolveMeasuredScrollViewSize(ScrollAxis axis, Size contentSize, LayoutConstraints const &constraints) {
    Size out = contentSize;

    switch (axis) {
    case ScrollAxis::Vertical:
        if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
            out.width = constraints.maxWidth;
        }
        if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
            out.height = std::min(out.height, constraints.maxHeight);
        }
        out.width = std::max(out.width, constraints.minWidth);
        break;
    case ScrollAxis::Horizontal:
        if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
            out.width = constraints.maxWidth;
        }
        if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
            out.height = std::min(out.height, constraints.maxHeight);
        }
        out.width = std::max(out.width, constraints.minWidth);
        break;
    case ScrollAxis::Both:
        if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
            out.width = std::min(out.width, constraints.maxWidth);
        }
        if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
            out.height = std::min(out.height, constraints.maxHeight);
        }
        break;
    }

    return out;
}

} // namespace

void ScrollView::layout(LayoutContext &ctx) const {
    ComponentKey const key = ctx.nextCompositeKey();
    StateStore *store = StateStore::current();
    detail::CompositeBodyResolution resolution{};
    if (store) {
        store->pushComponent(key, std::type_index(typeid(ScrollView)));
        store->pushCompositeConstraints(ctx.constraints());
        try {
            resolution = detail::resolveCompositeBody(store, key, ctx.constraints(), *this,
                                                      [&] { return body(); });
        } catch (...) {
            store->popCompositeConstraints();
            store->popComponent();
            throw;
        }
        store->popCompositeConstraints();
        store->popComponent();
    }
    Element &childEl = store ? *resolution.body : ctx.pinElement(body());
    ctx.beginCompositeBodySubtree(key);
    ctx.pushCompositeKeyTail(key);
    bool clonedRetainedSubtree = false;
    if (store) {
        Rect const assignedFrame = ctx.layoutEngine().lastAssignedFrame();
        store->recordBodyConstraints(key, ctx.constraints());
        store->recordLayoutBoundary(key, ctx.constraints(), assignedFrame);
        store->pushCompositePathStable(resolution.descendantsStable);
        clonedRetainedSubtree =
            resolution.descendantsStable && !store->hasDirtyDescendant(key) &&
            store->canReuseRetainedLayoutSubtree(key, ctx.constraints(), assignedFrame) &&
            ctx.cloneRetainedSubtree(key);
    }
    if (!clonedRetainedSubtree) {
        childEl.layout(ctx);
    }
    if (store) {
        store->popCompositePathStable();
    }
    ctx.popCompositeKeyTail();
}

void ScrollView::renderFromLayout(RenderContext &, LayoutNode const &) const {}

Size ScrollView::measure(LayoutContext &ctx, LayoutConstraints const &constraints, LayoutHints const &hints,
                         TextSystem &ts) const {
    ComponentKey const key = ctx.nextCompositeKey();
    ctx.beginCompositeBodySubtree(key);
    ctx.pushCompositeKeyTail(key);
    Element contentEl = OffsetView {
        .offset = Point {0.f, 0.f},
        .axis = axis,
        .children = children,
    };
    Size const bodySize = contentEl.measure(ctx, constraints, hints, ts);
    ctx.popCompositeKeyTail();
    return resolveMeasuredScrollViewSize(axis, bodySize, constraints);
}

} // namespace flux
