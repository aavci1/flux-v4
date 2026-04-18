#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <algorithm>

namespace flux {

void Spacer::layout(LayoutContext& ctx) const {
  ctx.advanceChildSlot();
  Rect const r = ctx.layoutEngine().lastAssignedFrame();
  float fg = 0.f;
  float fs = 0.f;
  float mm = 0.f;
  if (Element const* el = ctx.currentElement()) {
    fg = el->flexGrow();
    fs = el->flexShrink();
    mm = el->minMainSize();
  }
  layoutDebugLogLeaf("Spacer", ctx.constraints(), r, fg, fs, mm);
}

Size Spacer::measure(LayoutContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float m = 0.f;
  if (Element const* el = ctx.currentElement()) {
    m = std::max(0.f, el->minMainSize());
  }
  return {m, m};
}

} // namespace flux
