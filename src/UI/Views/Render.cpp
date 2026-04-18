#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/MeasureContext.hpp>

namespace flux {

Size Render::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                     TextSystem&) const {
  ctx.advanceChildSlot();
  return measure(constraints, hints);
}

} // namespace flux
