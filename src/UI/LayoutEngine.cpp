#include <Flux/UI/LayoutEngine.hpp>

#include <cassert>

namespace flux {

void LayoutEngine::resetForBuild() {
  childFrame_ = {};
#ifndef NDEBUG
  frameValid_ = false;
#endif
}

void LayoutEngine::setChildFrame(Rect frame) {
  childFrame_ = frame;
#ifndef NDEBUG
  frameValid_ = true;
#endif
}

Rect LayoutEngine::consumeAssignedFrame() {
#ifndef NDEBUG
  assert(frameValid_ && "LayoutEngine::consumeAssignedFrame: parent did not call setChildFrame for this child");
  frameValid_ = false;
#endif
  return childFrame_;
}

Rect LayoutEngine::lastAssignedFrame() const { return childFrame_; }

} // namespace flux
