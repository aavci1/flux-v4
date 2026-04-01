#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/TestAnnotate.hpp>

#include <algorithm>

namespace flux {

void Element::Model<Spacer>::build(BuildContext& ctx) const {
  ComponentKey const key = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const fr = ctx.layoutEngine().childFrame();
  detail::annotateLeaf(ctx, value, key, fr);
}

Size Element::Model<Spacer>::measure(BuildContext& ctx, LayoutConstraints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const m = std::max(0.f, value.minLength);
  return {m, m};
}

} // namespace flux
