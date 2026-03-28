#include <Flux/UI/LayoutEngine.hpp>

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/Graphics/TextSystem.hpp>

namespace flux {

void LayoutEngine::resetForBuild() { childFrame_ = {}; }

Size LayoutEngine::measure(BuildContext& ctx, Element const& element, LayoutConstraints const& constraints,
                             TextSystem& textSystem) const {
  return element.measure(ctx, constraints, textSystem);
}

void LayoutEngine::setChildFrame(Rect frame) { childFrame_ = frame; }

Rect LayoutEngine::childFrame() const { return childFrame_; }

} // namespace flux
