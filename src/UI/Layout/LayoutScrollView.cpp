#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cmath>

namespace flux {

void ScrollView::layout(LayoutContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  ComponentKey const key = ctx.nextCompositeKey();
  StateStore* store = StateStore::current();
  if (store) {
    store->pushComponent(key);
  }
  Element& childEl = ctx.pinElement(body());
  if (store) {
    store->popComponent();
  }
  ctx.beginCompositeBodySubtree(key);
  ctx.pushCompositeKeyTail(key);
  childEl.layout(ctx);
  ctx.popCompositeKeyTail();
}

void ScrollView::renderFromLayout(RenderContext&, LayoutNode const&) const {}

Size ScrollView::measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                         TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  ComponentKey const key = ctx.nextCompositeKey();
  StateStore* store = StateStore::current();
  if (store) {
    store->pushComponent(key);
  }
  Element& childEl = ctx.pinElement(body());
  if (store) {
    store->popComponent();
  }
  ctx.beginCompositeBodySubtree(key);
  ctx.pushCompositeKeyTail(key);
  Size const childSize = childEl.measure(ctx, constraints, hints, ts);
  ctx.popCompositeKeyTail();
  // Finite max = viewport from parent; infinity = unconstrained axis — use content intrinsic size
  // (auto-sized windows measure with both axes infinite; 0 here made the window 1×1 px).
  float const w = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : childSize.width;
  float const h = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : childSize.height;
  return {w, h};
}

} // namespace flux
