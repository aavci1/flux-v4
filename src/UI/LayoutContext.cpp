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

namespace {

bool constraintsEqual(LayoutConstraints const& a, LayoutConstraints const& b) {
  return a.minWidth == b.minWidth && a.minHeight == b.minHeight &&
         a.maxWidth == b.maxWidth && a.maxHeight == b.maxHeight;
}

bool hintsEqual(LayoutHints const& a, LayoutHints const& b) {
  return a.hStackCrossAlign == b.hStackCrossAlign && a.vStackCrossAlign == b.vStackCrossAlign;
}

bool rectEqual(Rect const& a, Rect const& b) {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

} // namespace

LayoutContext::LayoutContext(TextSystem& ts, LayoutEngine& layout, LayoutTree& tree, MeasureCache* measureCache,
                             SubtreeRootMap const* retainedRoots)
    : textSystem_(ts)
    , layoutEngine_(layout)
    , tree_(tree)
    , measureCache_(measureCache)
    , elementPins_(std::make_shared<detail::ElementPinStorage>())
    , retainedRoots_(retainedRoots) {
  layoutStack_.push_back(LayoutFrame{});
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

TextSystem& LayoutContext::textSystem() { return textSystem_; }

LayoutEngine& LayoutContext::layoutEngine() { return layoutEngine_; }

LayoutTree& LayoutContext::tree() { return tree_; }

LayoutTree const& LayoutContext::tree() const { return tree_; }

LayoutConstraints const& LayoutContext::constraints() const { return layoutStack_.back().constraints; }

LayoutHints const& LayoutContext::hints() const { return layoutStack_.back().hints; }

void LayoutContext::pushConstraints(LayoutConstraints const& c, LayoutHints hints) {
#ifndef NDEBUG
  assert(std::isfinite(c.minWidth) && std::isfinite(c.minHeight));
  assert(c.minWidth <= c.maxWidth);
  assert(c.minHeight <= c.maxHeight);
#endif
  layoutStack_.push_back(LayoutFrame{.constraints = c, .hints = std::move(hints)});
}

void LayoutContext::popConstraints() {
  if (layoutStack_.size() > 1) {
    layoutStack_.pop_back();
  }
}

void LayoutContext::pushChildIndex() {
  keyStack_.push_back(nextChildIndex_);
  savedChildIndices_.push_back(nextChildIndex_);
  nextChildIndex_ = 0;
}

void LayoutContext::popChildIndex() {
  keyStack_.pop_back();
  nextChildIndex_ = savedChildIndices_.back();
  savedChildIndices_.pop_back();
  ++nextChildIndex_;
}

void LayoutContext::setChildIndex(std::size_t index) { nextChildIndex_ = index; }

ComponentKey LayoutContext::nextCompositeKey() {
  ComponentKey key = keyStack_;
  key.push_back(nextChildIndex_++);
  return key;
}

ComponentKey LayoutContext::peekNextCompositeKey() const {
  ComponentKey key = keyStack_;
  key.push_back(nextChildIndex_);
  return key;
}

void LayoutContext::advanceChildSlot() { ++nextChildIndex_; }

ComponentKey LayoutContext::leafComponentKey() const {
  ComponentKey key = keyStack_;
  key.push_back(nextChildIndex_);
  return key;
}

void LayoutContext::rewindChildKeyIndex() { nextChildIndex_ = 0; }

void LayoutContext::beginCompositeBodySubtree(ComponentKey compositeKey) {
  skipNextLayoutChildAdvance_ = true;
  pendingCompositeSubtreeRoot_ = true;
  pendingCompositeSubtreeKey_ = std::move(compositeKey);
}

bool LayoutContext::consumeCompositeBodySubtreeRootSkip() {
  if (skipNextLayoutChildAdvance_) {
    skipNextLayoutChildAdvance_ = false;
    return true;
  }
  return false;
}

void LayoutContext::pushCompositeKeyTail(ComponentKey const& compositeKey) {
  assert(!compositeKey.empty());
  keyStack_.push_back(compositeKey.back());
  savedChildIndices_.push_back(nextChildIndex_);
  nextChildIndex_ = 0;
}

void LayoutContext::popCompositeKeyTail() {
  assert(!keyStack_.empty());
  keyStack_.pop_back();
  assert(!savedChildIndices_.empty());
  nextChildIndex_ = savedChildIndices_.back();
  savedChildIndices_.pop_back();
}

void LayoutContext::registerCompositeSubtreeRootIfPending(LayoutNodeId layoutNodeId) {
  if (!pendingCompositeSubtreeRoot_) {
    return;
  }
  pendingCompositeSubtreeRoot_ = false;
  subtreeRootLayouts_[pendingCompositeSubtreeKey_] = layoutNodeId;
}

MeasureCache* LayoutContext::measureCache() const { return measureCache_; }

LayoutContext::SubtreeRootMap const& LayoutContext::subtreeRootLayouts() const {
  return subtreeRootLayouts_;
}

bool LayoutContext::canReuseRetainedCompositeSubtree(ComponentKey const& compositeKey, Rect const& assignedFrame,
                                                     LayoutConstraints const& constraints,
                                                     LayoutHints const& hints) const {
  if (!retainedRoots_) {
    return false;
  }
  auto const it = retainedRoots_->find(compositeKey);
  if (it == retainedRoots_->end()) {
    return false;
  }
  LayoutNode const* root = tree_.get(it->second);
  if (!root) {
    return false;
  }
  if (!constraintsEqual(root->constraints, constraints) || !hintsEqual(root->hints, hints)) {
    return false;
  }
  if (rectEqual(root->assignedFrame, assignedFrame)) {
    return true;
  }
  return root->assignedFrame.width == assignedFrame.width &&
         root->assignedFrame.height == assignedFrame.height &&
         tree_.canTranslateSubtree(it->second);
}

bool LayoutContext::reuseRetainedCompositeSubtree(ComponentKey const& compositeKey, Rect const& assignedFrame) {
  if (!retainedRoots_) {
    return false;
  }
  auto const it = retainedRoots_->find(compositeKey);
  if (it == retainedRoots_->end()) {
    return false;
  }
  LayoutNodeId const rootId = it->second;
  if (!tree_.reuseSubtree(rootId, currentLayoutParent())) {
    return false;
  }
  if (LayoutNode* root = tree_.get(rootId)) {
    Vec2 const delta{assignedFrame.x - root->assignedFrame.x, assignedFrame.y - root->assignedFrame.y};
    if (delta.x != 0.f || delta.y != 0.f) {
      tree_.translateSubtree(rootId, delta);
    }
  }
  subtreeRootLayouts_[compositeKey] = rootId;
  pendingCompositeSubtreeRoot_ = false;
  return true;
}

void LayoutContext::pushActiveElementModifiers(ElementModifiers const* m) { activeElementModifiers_.push_back(m); }

void LayoutContext::popActiveElementModifiers() {
#ifndef NDEBUG
  assert(!activeElementModifiers_.empty());
#endif
  activeElementModifiers_.pop_back();
}

ElementModifiers const* LayoutContext::activeElementModifiers() const noexcept {
  return activeElementModifiers_.empty() ? nullptr : activeElementModifiers_.back();
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
