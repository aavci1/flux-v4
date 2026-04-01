#include <Flux/UI/BuildContext.hpp>

#include <Flux/UI/TestTreeAnnotator.hpp>

#include <cassert>
#include <unordered_map>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/EventMap.hpp>

namespace flux {

BuildContext::BuildContext(SceneGraph& g, EventMap& em, TextSystem& ts, LayoutEngine& layout,
                           MeasureCache* measureCache, TestTreeAnnotator* testAnnotator)
    : graph_(g)
    , eventMap_(em)
    , textSystem_(ts)
    , layoutEngine_(layout)
    , measureCache_(measureCache)
    , testAnnotator_(testAnnotator) {
  constraintStack_.push_back(LayoutConstraints{});
}

NodeId BuildContext::parentLayer() const {
  if (layerStack_.empty()) {
    return graph_.root();
  }
  return layerStack_.back();
}

SceneGraph& BuildContext::graph() { return graph_; }

EventMap& BuildContext::eventMap() { return eventMap_; }

TextSystem& BuildContext::textSystem() { return textSystem_; }

LayoutEngine& BuildContext::layoutEngine() { return layoutEngine_; }

TestTreeAnnotator* BuildContext::testAnnotator() const noexcept { return testAnnotator_; }

void BuildContext::pushLayer(NodeId layerId) { layerStack_.push_back(layerId); }

void BuildContext::popLayer() {
  if (!layerStack_.empty()) {
    layerStack_.pop_back();
  }
}

LayoutConstraints const& BuildContext::constraints() const { return constraintStack_.back(); }

void BuildContext::pushConstraints(LayoutConstraints const& c) { constraintStack_.push_back(c); }

void BuildContext::popConstraints() {
  if (constraintStack_.size() > 1) {
    constraintStack_.pop_back();
  }
}

void BuildContext::pushChildIndex() {
  keyStack_.push_back(nextChildIndex_);
  savedChildIndices_.push_back(nextChildIndex_);
  nextChildIndex_ = 0;
}

void BuildContext::popChildIndex() {
  keyStack_.pop_back();
  nextChildIndex_ = savedChildIndices_.back();
  savedChildIndices_.pop_back();
  ++nextChildIndex_;
}

void BuildContext::setChildIndex(std::size_t index) {
  nextChildIndex_ = index;
}

ComponentKey BuildContext::nextCompositeKey() {
  ComponentKey key = keyStack_;
  key.push_back(nextChildIndex_++);
  return key;
}

void BuildContext::advanceChildSlot() { ++nextChildIndex_; }

ComponentKey BuildContext::leafComponentKey() const {
  ComponentKey key = keyStack_;
  key.push_back(nextChildIndex_);
  return key;
}

void BuildContext::rewindChildKeyIndex() { nextChildIndex_ = 0; }

void BuildContext::beginCompositeBodySubtree(ComponentKey compositeKey) {
  skipNextLayoutChildAdvance_ = true;
  pendingCompositeSubtreeRoot_ = true;
  pendingCompositeSubtreeKey_ = std::move(compositeKey);
}

bool BuildContext::consumeCompositeBodySubtreeRootSkip() {
  if (skipNextLayoutChildAdvance_) {
    skipNextLayoutChildAdvance_ = false;
    return true;
  }
  return false;
}

void BuildContext::pushCompositeKeyTail(ComponentKey const& compositeKey) {
  assert(!compositeKey.empty());
  keyStack_.push_back(compositeKey.back());
  savedChildIndices_.push_back(nextChildIndex_);
  nextChildIndex_ = 0;
}

void BuildContext::popCompositeKeyTail() {
  assert(!keyStack_.empty());
  keyStack_.pop_back();
  assert(!savedChildIndices_.empty());
  nextChildIndex_ = savedChildIndices_.back();
  savedChildIndices_.pop_back();
}

void BuildContext::registerCompositeSubtreeRootIfPending(NodeId layerId) {
  if (!pendingCompositeSubtreeRoot_) {
    return;
  }
  pendingCompositeSubtreeRoot_ = false;
  subtreeRootLayers_[pendingCompositeSubtreeKey_] = layerId;
}

MeasureCache* BuildContext::measureCache() const {
  return measureCache_;
}

std::unordered_map<ComponentKey, NodeId, ComponentKeyHash> const& BuildContext::subtreeRootLayers() const {
  return subtreeRootLayers_;
}

} // namespace flux
