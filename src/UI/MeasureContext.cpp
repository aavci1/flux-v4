#include <Flux/UI/MeasureContext.hpp>

#include <cassert>
#include <cmath>

#include <Flux/Graphics/TextSystem.hpp>

namespace flux {

MeasureContext::MeasureContext(TextSystem& ts, MeasureCache* measureCache)
    : textSystem_(ts)
    , measureCache_(measureCache) {
  layoutStack_.push_back(LayoutFrame{});
}

MeasureContext::~MeasureContext() = default;

TextSystem& MeasureContext::textSystem() { return textSystem_; }

MeasureCache* MeasureContext::measureCache() const { return measureCache_; }

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

void MeasureContext::pushChildIndex() {
  keyStack_.push_back(nextChildIndex_);
  savedChildIndices_.push_back(nextChildIndex_);
  nextChildIndex_ = 0;
}

void MeasureContext::popChildIndex() {
  keyStack_.pop_back();
  nextChildIndex_ = savedChildIndices_.back();
  savedChildIndices_.pop_back();
  ++nextChildIndex_;
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
  ComponentKey key = keyStack_;
  key.push_back(currentChildLocalId());
  ++nextChildIndex_;
  return key;
}

ComponentKey MeasureContext::peekNextCompositeKey() const {
  ComponentKey key = keyStack_;
  key.push_back(currentChildLocalId());
  return key;
}

void MeasureContext::advanceChildSlot() { ++nextChildIndex_; }

ComponentKey MeasureContext::leafComponentKey() const {
  ComponentKey key = keyStack_;
  key.push_back(currentChildLocalId());
  return key;
}

void MeasureContext::rewindChildKeyIndex() { nextChildIndex_ = 0; }

void MeasureContext::resetTraversalState(ComponentKey const& key) {
  keyStack_.clear();
  savedChildIndices_.clear();
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
  assert(!compositeKey.empty());
  keyStack_.push_back(compositeKey.back());
  savedChildIndices_.push_back(nextChildIndex_);
  nextChildIndex_ = 0;
}

void MeasureContext::popCompositeKeyTail() {
  assert(!keyStack_.empty());
  keyStack_.pop_back();
  assert(!savedChildIndices_.empty());
  nextChildIndex_ = savedChildIndices_.back();
  savedChildIndices_.pop_back();
}

void MeasureContext::pushActiveElementModifiers(ElementModifiers const* m) {
  activeElementModifiers_.push_back(m);
}

void MeasureContext::popActiveElementModifiers() {
#ifndef NDEBUG
  assert(!activeElementModifiers_.empty());
#endif
  activeElementModifiers_.pop_back();
}

ElementModifiers const* MeasureContext::activeElementModifiers() const noexcept {
  return activeElementModifiers_.empty() ? nullptr : activeElementModifiers_.back();
}

} // namespace flux
