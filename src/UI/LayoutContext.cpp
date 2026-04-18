#include <Flux/UI/LayoutContext.hpp>

#include <Flux/UI/Element.hpp>

#include <cassert>
#include <cmath>
#include <deque>

#include <Flux/Graphics/TextSystem.hpp>

namespace flux {

namespace detail {

struct ElementPinStorage {
  std::deque<Element> pins{};
};

} // namespace detail
LayoutContext::LayoutContext(TextSystem& ts, LayoutEngine& layout, LayoutTree& tree, MeasureCache* measureCache)
    : MeasureContext(ts, measureCache)
    , layoutEngine_(layout)
    , tree_(tree)
    , elementPins_(std::make_shared<detail::ElementPinStorage>()) {
  layerWorldStack_.push_back(Mat3::identity());
}

LayoutContext::~LayoutContext() = default;

Element& LayoutContext::pinElement(Element&& el) {
  elementPins_->pins.push_back(std::move(el));
  return elementPins_->pins.back();
}

std::shared_ptr<detail::ElementPinStorage> LayoutContext::pinnedElements() const noexcept {
  return elementPins_;
}
LayoutEngine& LayoutContext::layoutEngine() { return layoutEngine_; }

LayoutTree& LayoutContext::tree() { return tree_; }

LayoutTree const& LayoutContext::tree() const { return tree_; }

void LayoutContext::registerCompositeSubtreeRootIfPending(LayoutNodeId layoutNodeId) {
  (void)layoutNodeId;
}

void LayoutContext::pushLayerWorldTransform(Mat3 const& localToParentLayer) {
  Mat3 const combined = layerWorldStack_.back() * localToParentLayer;
  layerWorldStack_.push_back(combined);
}

void LayoutContext::popLayerWorldTransform() {
#ifndef NDEBUG
  assert(layerWorldStack_.size() > 1);
#endif
  layerWorldStack_.pop_back();
}

Mat3 LayoutContext::currentLayerWorldTransform() const { return layerWorldStack_.back(); }

void LayoutContext::pushLayoutParent(LayoutNodeId id) { layoutParentStack_.push_back(id); }

void LayoutContext::popLayoutParent() {
#ifndef NDEBUG
  assert(!layoutParentStack_.empty());
#endif
  layoutParentStack_.pop_back();
}

LayoutNodeId LayoutContext::currentLayoutParent() const {
  return layoutParentStack_.empty() ? LayoutNodeId{} : layoutParentStack_.back();
}

LayoutNodeId LayoutContext::pushLayoutNode(LayoutNode&& node) {
  node.assignedFrame = layoutEngine_.lastAssignedFrame();
  node.worldBounds = transformWorldBounds(currentLayerWorldTransform(), node.frame);
  return tree_.pushNode(std::move(node), currentLayoutParent());
}

} // namespace flux
