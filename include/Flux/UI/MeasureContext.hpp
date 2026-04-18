#pragma once

/// \file Flux/UI/MeasureContext.hpp
///
/// Context for \ref Element::measure during retained-scene rebuilds.

#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace flux {

class TextSystem;
struct ElementModifiers;
class Element;

class MeasureContext {
public:
  explicit MeasureContext(TextSystem& ts);
  ~MeasureContext();

  TextSystem& textSystem();

  LayoutConstraints const& constraints() const;
  LayoutHints const& hints() const;
  void pushConstraints(LayoutConstraints const& c, LayoutHints hints = {});
  void popConstraints();

  void pushChildIndex();
  void popChildIndex();

  void setChildIndex(std::size_t index);
  void pushExplicitChildLocalId(std::optional<LocalId> localId);
  void popExplicitChildLocalId();

  ComponentKey nextCompositeKey();
  ComponentKey peekNextCompositeKey() const;

  void advanceChildSlot();
  ComponentKey leafComponentKey() const;
  void rewindChildKeyIndex();
  void resetTraversalState(ComponentKey const& key = {});

  void beginCompositeBodySubtree(ComponentKey compositeKey);
  bool consumeCompositeBodySubtreeRootSkip();

  void pushCompositeKeyTail(ComponentKey const& compositeKey);
  void popCompositeKeyTail();

  void setCurrentElement(Element const* el) noexcept { currentElement_ = el; }
  [[nodiscard]] Element const* currentElement() const noexcept { return currentElement_; }

#ifndef NDEBUG
  std::size_t debugConstraintStackDepth() const noexcept { return layoutStack_.size(); }
  std::size_t debugKeyPathDepth() const noexcept { return keyStack_.size(); }
  std::size_t debugSavedChildDepth() const noexcept { return savedChildIndices_.size(); }
#endif

protected:
  struct LayoutFrame {
    LayoutConstraints constraints{};
    LayoutHints hints{};
  };

  TextSystem& textSystem_;
  std::vector<LayoutFrame> layoutStack_;

  std::vector<LocalId> keyStack_;
  std::vector<std::optional<LocalId>> explicitChildLocalIdStack_;
  std::vector<std::size_t> savedChildIndices_;
  std::size_t nextChildIndex_{0};
  bool skipNextLayoutChildAdvance_{false};
  Element const* currentElement_{nullptr};

private:
  [[nodiscard]] LocalId currentChildLocalId() const;
};

} // namespace flux
