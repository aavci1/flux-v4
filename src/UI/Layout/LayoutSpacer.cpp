#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/Spacer.hpp>

#include <algorithm>

namespace flux {

Size Spacer::measure(MeasureContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float m = 0.f;
  if (Element const* el = ctx.currentElement()) {
    m = std::max(0.f, el->minMainSize());
  }
  return {m, m};
}

} // namespace flux
