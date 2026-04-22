#include <Flux/UI/MeasureContext.hpp>

#include <cassert>
#include <cmath>

#include <Flux/Graphics/TextSystem.hpp>

namespace flux {

MeasureContext::MeasureContext(TextSystem& ts, detail::MeasureLayoutCache* layoutCache)
    : textSystem_(ts)
    , layoutCache_(layoutCache) {
  layoutStack_.push_back(LayoutFrame{});
}

MeasureContext::~MeasureContext() = default;

TextSystem& MeasureContext::textSystem() { return textSystem_; }

LayoutConstraints const& MeasureContext::constraints() const { return layoutStack_.back().constraints; }

LayoutHints const& MeasureContext::hints() const { return layoutStack_.back().hints; }

void MeasureContext::pushConstraints(LayoutConstraints const& c, LayoutHints hints) {
#ifndef NDEBUG
  assert(std::isfinite(c.minWidth) && std::isfinite(c.minHeight));
  assert(c.minWidth <= c.maxWidth);
  assert(c.minHeight <= c.maxHeight);
#endif
  layoutStack_.push_back(LayoutFrame{.constraints = c, .hints = std::move(hints)});
}

void MeasureContext::popConstraints() {
  if (layoutStack_.size() > 1) {
    layoutStack_.pop_back();
  }
}

void MeasureContext::pushChildIndex(bool pushKeySegment) {
  if (pushKeySegment) {
    keyStack_.push_back(nextChildIndex_);
  }
  savedChildIndices_.push_back(nextChildIndex_);
  pushedChildIndexKeyStack_.push_back(pushKeySegment);
  nextChildIndex_ = 0;
}

void MeasureContext::pushChildIndexWithLocalId(LocalId localId) {
  keyStack_.push_back(localId);
  savedChildIndices_.push_back(nextChildIndex_);
  pushedChildIndexKeyStack_.push_back(true);
  nextChildIndex_ = 0;
}

void MeasureContext::popChildIndex() {
  assert(!pushedChildIndexKeyStack_.empty());
  if (pushedChildIndexKeyStack_.back()) {
    keyStack_.pop_back();
  }
  pushedChildIndexKeyStack_.pop_back();
  nextChildIndex_ = savedChildIndices_.back();
  savedChildIndices_.pop_back();
}

void MeasureContext::setChildIndex(std::size_t index) { nextChildIndex_ = index; }

void MeasureContext::pushExplicitChildLocalId(std::optional<LocalId> localId) {
  explicitChildLocalIdStack_.push_back(std::move(localId));
}

void MeasureContext::popExplicitChildLocalId() {
#ifndef NDEBUG
  assert(!explicitChildLocalIdStack_.empty());
#endif
  explicitChildLocalIdStack_.pop_back();
}

LocalId MeasureContext::currentChildLocalId() const {
  if (!explicitChildLocalIdStack_.empty() && explicitChildLocalIdStack_.back().has_value()) {
    return *explicitChildLocalIdStack_.back();
  }
  return LocalId::fromIndex(nextChildIndex_);
}

ComponentKey MeasureContext::nextCompositeKey() {
  if (useMeasurementRootKey_) {
    useMeasurementRootKey_ = false;
    return measurementRootKey_;
  }
  ComponentKey key = keyStack_;
  key.push_back(currentChildLocalId());
  ++nextChildIndex_;
  return key;
}

ComponentKey MeasureContext::peekNextCompositeKey() const {
  if (useMeasurementRootKey_) {
    return measurementRootKey_;
  }
  ComponentKey key = keyStack_;
  key.push_back(currentChildLocalId());
  return key;
}

void MeasureContext::advanceChildSlot() { ++nextChildIndex_; }

ComponentKey MeasureContext::currentElementKey() const {
  if (skipNextLayoutChildAdvance_) {
    return keyStack_;
  }
  ComponentKey key = keyStack_;
  key.push_back(currentChildLocalId());
  return key;
}

ComponentKey MeasureContext::leafComponentKey() const {
  ComponentKey key = keyStack_;
  key.push_back(currentChildLocalId());
  return key;
}

void MeasureContext::rewindChildKeyIndex() { nextChildIndex_ = 0; }

void MeasureContext::resetTraversalState(ComponentKey const& key) {
  measurementRootKey_ = key;
  useMeasurementRootKey_ = false;
  keyStack_.clear();
  savedChildIndices_.clear();
  pushedChildIndexKeyStack_.clear();
  pushedCompositeKeyTailStack_.clear();
  explicitChildLocalIdStack_.clear();
  nextChildIndex_ = 0;
  if (!key.empty()) {
    for (std::size_t i = 0; i + 1 < key.size(); ++i) {
      keyStack_.push_back(key[i]);
    }
    LocalId const local = key.back();
    if (local.kind == LocalId::Kind::Positional) {
      nextChildIndex_ = local.value == 0 ? 0u : static_cast<std::size_t>(local.value - 1ull);
    } else {
      explicitChildLocalIdStack_.push_back(local);
    }
  }
}

void MeasureContext::setMeasurementRootKey(ComponentKey key) {
  measurementRootKey_ = std::move(key);
  useMeasurementRootKey_ = true;
}

void MeasureContext::clearMeasurementRootKey() noexcept {
  useMeasurementRootKey_ = false;
}

void MeasureContext::beginCompositeBodySubtree(ComponentKey compositeKey) {
  (void)compositeKey;
  skipNextLayoutChildAdvance_ = true;
}

bool MeasureContext::consumeCompositeBodySubtreeRootSkip() {
  if (skipNextLayoutChildAdvance_) {
    skipNextLayoutChildAdvance_ = false;
    return true;
  }
  return false;
}

void MeasureContext::pushCompositeKeyTail(ComponentKey const& compositeKey) {
  savedChildIndices_.push_back(nextChildIndex_);
  bool const pushedKeyTail = !compositeKey.empty();
  pushedCompositeKeyTailStack_.push_back(pushedKeyTail);
  if (pushedKeyTail) {
    keyStack_.push_back(compositeKey.back());
  }
  nextChildIndex_ = 0;
}

void MeasureContext::popCompositeKeyTail() {
  assert(!pushedCompositeKeyTailStack_.empty());
  if (pushedCompositeKeyTailStack_.back()) {
    assert(!keyStack_.empty());
    keyStack_.pop_back();
  }
  pushedCompositeKeyTailStack_.pop_back();
  assert(!savedChildIndices_.empty());
  nextChildIndex_ = savedChildIndices_.back();
  savedChildIndices_.pop_back();
}

} // namespace flux
