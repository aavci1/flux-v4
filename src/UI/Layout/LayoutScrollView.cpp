#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/TestAnnotate.hpp>

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
  ctx.beginCompositeBodySubtree(key);
  ctx.pushCompositeKeyTail(key);
  detail::annotateCompositeEnter(ctx, value, key);
  child.build(ctx);
  ctx.popCompositeKeyTail();
  detail::annotateCompositeExit(ctx);
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
  ctx.beginCompositeBodySubtree(key);
  ctx.pushCompositeKeyTail(key);
  (void)child.measure(ctx, constraints, ts);
  ctx.popCompositeKeyTail();
  float const w = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const h = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  return {w, h};
}

} // namespace flux
