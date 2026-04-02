#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <algorithm>

namespace flux {

void Spacer::build(BuildContext& ctx) const {
  ctx.advanceChildSlot();
  Rect const r = ctx.layoutEngine().lastAssignedFrame();
  layoutDebugLogLeaf("Spacer", ctx.constraints(), r, flexGrow, flexShrink, std::max(0.f, minLength));
}

Size Spacer::measure(BuildContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const m = std::max(0.f, minLength);
  return {m, m};
}

} // namespace flux
