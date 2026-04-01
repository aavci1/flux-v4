#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <algorithm>

namespace flux {

void Element::Model<Spacer>::build(BuildContext& ctx) const {
  ctx.advanceChildSlot();
  Rect const r = ctx.layoutEngine().lastAssignedFrame();
  layoutDebugLogLeaf("Spacer", ctx.constraints(), r, 1.f, 0.f, std::max(0.f, value.minLength));
}

Size Element::Model<Spacer>::measure(BuildContext& ctx, LayoutConstraints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const m = std::max(0.f, value.minLength);
  return {m, m};
}

} // namespace flux
