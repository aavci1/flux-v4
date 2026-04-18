#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Views/OffsetView.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

namespace {

struct OffsetViewport {
    float innerW = 0.f;
    float innerH = 0.f;
    float viewportW = 0.f;
    float viewportH = 0.f;
};

OffsetViewport resolveViewport(float assignedW, float assignedH, LayoutConstraints const &outer) {
    OffsetViewport viewport {
        .innerW = std::max(0.f, assignedW),
        .innerH = std::max(0.f, assignedH),
        .viewportW = std::max(0.f, assignedW),
        .viewportH = std::max(0.f, assignedH),
    };

    if (viewport.viewportW <= 0.f && std::isfinite(outer.maxWidth) && outer.maxWidth > 0.f) {
        viewport.viewportW = outer.maxWidth;
    }
    if (viewport.viewportH <= 0.f && std::isfinite(outer.maxHeight) && outer.maxHeight > 0.f) {
        viewport.viewportH = outer.maxHeight;
    }

    return viewport;
}

LayoutConstraints scrollChildConstraints(ScrollAxis axis, LayoutConstraints constraints, float viewportW, float viewportH) {
    switch (axis) {
    case ScrollAxis::Vertical:
        constraints.maxWidth = viewportW > 0.f ? viewportW : std::numeric_limits<float>::infinity();
        constraints.maxHeight = std::numeric_limits<float>::infinity();
        break;
    case ScrollAxis::Horizontal:
        constraints.maxWidth = std::numeric_limits<float>::infinity();
        constraints.maxHeight = viewportH > 0.f ? viewportH : std::numeric_limits<float>::infinity();
        break;
    case ScrollAxis::Both:
        constraints.maxWidth = std::numeric_limits<float>::infinity();
        constraints.maxHeight = std::numeric_limits<float>::infinity();
        break;
    }
    clampLayoutMinToMax(constraints);
    return constraints;
}

Size scrollContentSize(ScrollAxis axis, std::vector<Size> const &sizes) {
    float totalW = 0.f;
    float totalH = 0.f;

    switch (axis) {
    case ScrollAxis::Horizontal:
        for (Size const s : sizes) {
            totalW += s.width;
            totalH = std::max(totalH, s.height);
        }
        break;
    case ScrollAxis::Vertical:
        for (Size const s : sizes) {
            totalW = std::max(totalW, s.width);
            totalH += s.height;
        }
        break;
    case ScrollAxis::Both:
        for (Size const s : sizes) {
            totalW = std::max(totalW, s.width);
            totalH = std::max(totalH, s.height);
        }
        break;
    }

    return {totalW, totalH};
}

bool sizeApproximatelyEqual(Size const &a, Size const &b, float tolerance = 0.5f) {
    return std::abs(a.width - b.width) <= tolerance && std::abs(a.height - b.height) <= tolerance;
}

} // namespace

void OffsetView::layout(LayoutContext &ctx) const {
    ContainerLayoutScope scope(ctx);
    float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
    float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
    OffsetViewport const viewport = resolveViewport(assignedW, assignedH, scope.outer);

    if (viewportSize.signal) {
        Size const nextViewport {viewport.viewportW, viewport.viewportH};
        if (!sizeApproximatelyEqual(*viewportSize, nextViewport)) {
            viewportSize = nextViewport;
        }
    }

    LayoutConstraints const childCs = scrollChildConstraints(axis, scope.outer, viewport.viewportW, viewport.viewportH);

    auto sizes = scope.measureChildren(children, childCs);
    scope.logContainer("OffsetView");

    std::size_t const n = children.size();
    Size const contentExtent = scrollContentSize(axis, sizes);

    if (contentSize.signal) {
        if (!sizeApproximatelyEqual(*contentSize, contentExtent)) {
            contentSize = contentExtent;
        }
    }

    scope.pushOffsetScrollLayer(offset);

    if (axis == ScrollAxis::Horizontal) {
        float x = 0.f;
        for (std::size_t i = 0; i < n; ++i) {
            Size const sz = sizes[i];
            float const rowH = (viewport.viewportH > 0.f && std::isfinite(viewport.viewportH)) ? viewport.viewportH : std::max(sz.height, viewport.innerH);
            LayoutConstraints childBuild = scope.outer;
            childBuild.maxWidth = sz.width;
            childBuild.maxHeight = rowH;
            clampLayoutMinToMax(childBuild);
            scope.layoutChild(children[i], Rect {x, 0.f, sz.width, rowH}, childBuild);
            x += sz.width;
        }
    } else if (axis == ScrollAxis::Vertical) {
        float y = 0.f;
        for (std::size_t i = 0; i < n; ++i) {
            Size const sz = sizes[i];
            float const rowW = (viewport.viewportW > 0.f && std::isfinite(viewport.viewportW)) ? viewport.viewportW : std::max(sz.width, viewport.innerW);
            LayoutConstraints childBuild = scope.outer;
            childBuild.maxWidth = rowW;
            childBuild.maxHeight = sz.height;
            clampLayoutMinToMax(childBuild);
            scope.layoutChild(children[i], Rect {0.f, y, rowW, sz.height}, childBuild);
            y += sz.height;
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            Size const sz = sizes[i];
            LayoutConstraints childBuild = scope.outer;
            childBuild.maxWidth = sz.width;
            childBuild.maxHeight = sz.height;
            clampLayoutMinToMax(childBuild);
            scope.layoutChild(children[i], Rect {0.f, 0.f, sz.width, sz.height}, childBuild);
        }
    }
}

Size OffsetView::measure(MeasureContext &ctx, LayoutConstraints const &constraints, LayoutHints const &,
                         TextSystem &ts) const {
    ContainerMeasureScope scope(ctx);
    float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
    float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
    OffsetViewport const viewport = resolveViewport(assignedW, assignedH, constraints);
    LayoutConstraints const childCs = scrollChildConstraints(axis, constraints, viewport.viewportW, viewport.viewportH);

    std::vector<Size> sizes;
    sizes.reserve(children.size());
    for (Element const &ch : children) {
        sizes.push_back(ch.measure(ctx, childCs, LayoutHints {}, ts));
    }

    return scrollContentSize(axis, sizes);
}

} // namespace flux
