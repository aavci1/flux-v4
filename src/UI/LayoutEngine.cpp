#include <Flux/UI/LayoutEngine.hpp>

namespace flux {

void LayoutEngine::resetForBuild() { childFrame_ = {}; }

void LayoutEngine::setChildFrame(Rect frame) { childFrame_ = frame; }

Rect LayoutEngine::childFrame() const { return childFrame_; }

} // namespace flux
