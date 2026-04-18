#pragma once

/// \file Flux/UI/Views/ScrollView.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/OffsetView.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <optional>
#include <vector>

namespace flux {

/// Clamps \p o so the scrolled content does not overscroll past the viewport for \p axis.
/// Non-scrolling axes are zeroed (horizontal-only keeps `o.y == 0`, vertical-only keeps `o.x == 0`).
Point clampScrollOffset(ScrollAxis axis, Point o, Size const &viewport, Size const &content);

/// Scrollable region: children are laid out in an \ref OffsetView and can be dragged or wheel-scrolled.
struct ScrollView : ViewModifiers<ScrollView> {
    // ── Layout / axis ─────────────────────────────────────────────────────────

    ScrollAxis axis = ScrollAxis::Vertical;
    State<Point> scrollOffset {};
    State<Size> viewportSize {};
    State<Size> contentSize {};
    bool dragScrollEnabled = true;
    std::vector<Element> children;

    /// Custom subtree hook (not the generic \ref CompositeComponent path in \ref Element::Model).
    void layout(LayoutContext &) const;
    Size measure(MeasureContext &, LayoutConstraints const &, LayoutHints const &, TextSystem &) const;

    // ── Component protocol ─────────────────────────────────────────────────────

    Element body() const;
};

// `ScrollView` is a composite (`body()`) but uses a dedicated `Element::Model` that matches this
// subtree protocol instead of the default composite `Model<C>` implementation.
template <>
struct Element::Model<ScrollView> final : Element::Concept {
    ScrollView value;
    explicit Model(ScrollView c) : value(std::move(c)) {}
    std::unique_ptr<Concept> clone() const override {
        return std::make_unique<Model<ScrollView>>(value);
    }
    ElementType elementType() const noexcept override { return ElementType::ScrollView; }
    std::type_index modelType() const noexcept override { return std::type_index(typeid(ScrollView)); }
    void const* rawValuePtr() const noexcept override { return &value; }
    bool isComposite() const noexcept override { return true; }
    std::unique_ptr<Element> buildCompositeBody() const override {
        return std::make_unique<Element>(value.body());
    }
    void layout(LayoutContext &ctx) const override { value.layout(ctx); }
    Size measure(MeasureContext &ctx, LayoutConstraints const &c, LayoutHints const &h,
                 TextSystem &ts) const override {
        return value.measure(ctx, c, h, ts);
    }
    /// `ScrollView` keeps its measured viewport size by default; opt into fill with `.flex(...)`.
    float flexGrow() const override { return 0.f; }
    float flexShrink() const override { return 0.f; }
    float minMainSize() const override { return 0.f; }
    bool canMemoizeMeasure() const override { return false; }
};

} // namespace flux
