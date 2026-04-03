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
  (void)childEl.measure(ctx, constraints, hints, ts);
  ctx.popCompositeKeyTail();
  float const w = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const h = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  return {w, h};
}

} // namespace flux
