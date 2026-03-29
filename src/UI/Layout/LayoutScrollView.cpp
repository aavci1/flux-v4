#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cmath>

namespace flux {

void Element::Model<ScrollView>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  ComponentKey const key = ctx.nextCompositeKey();
  StateStore* store = StateStore::current();
  if (store) {
    store->pushComponent(key);
  }
  Element child{value.body()};
  if (store) {
    store->popComponent();
  }
  ctx.beginCompositeBodySubtree();
  child.build(ctx);
}

Size Element::Model<ScrollView>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                         TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  ComponentKey const key = ctx.nextCompositeKey();
  StateStore* store = StateStore::current();
  if (store) {
    store->pushComponent(key);
  }
  Element child{value.body()};
  if (store) {
    store->popComponent();
  }
  ctx.beginCompositeBodySubtree();
  (void)child.measure(ctx, constraints, ts);
  float const w = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const h = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  return {w, h};
}

} // namespace flux
