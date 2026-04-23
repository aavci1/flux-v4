#pragma once

/// \file Flux/UI/MeasureContext.hpp
///
/// Context for \ref Element::measure during retained-scene rebuilds.

#include <Flux/Core/ComponentKey.hpp>
#include <Flux/UI/Detail/TraversalContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstddef>
#include <optional>

namespace flux {

class TextSystem;
class Element;
namespace detail {
struct ElementModifiers;
class MeasureLayoutCache;
}

class MeasureContext {
public:
  explicit MeasureContext(TextSystem& ts, detail::MeasureLayoutCache* layoutCache = nullptr);
  ~MeasureContext();

  TextSystem& textSystem();

  LayoutConstraints const& constraints() const;
  LayoutHints const& hints() const;
  void pushConstraints(LayoutConstraints const& c, LayoutHints hints = {});
  void popConstraints();

  void pushChildIndex(bool pushKeySegment = true);
  void pushChildIndexWithLocalId(LocalId localId);
  void popChildIndex();

  void setChildIndex(std::size_t index);
  void pushExplicitChildLocalId(std::optional<LocalId> localId);
  void popExplicitChildLocalId();

  ComponentKey nextCompositeKey();

  void advanceChildSlot();
  ComponentKey currentElementKey() const;
  void rewindChildKeyIndex();
  void resetTraversalState(ComponentKey const& key = {});
  void setMeasurementRootKey(ComponentKey key);
  void clearMeasurementRootKey() noexcept;

  void beginCompositeBodySubtree(ComponentKey compositeKey);
  bool consumeCompositeBodySubtreeRootSkip();

  void pushCompositeKeyTail(ComponentKey const& compositeKey);
  void popCompositeKeyTail();

  void setCurrentElement(Element const* el) noexcept { currentElement_ = el; }
  [[nodiscard]] Element const* currentElement() const noexcept { return currentElement_; }
  [[nodiscard]] detail::MeasureLayoutCache* layoutCache() const noexcept { return layoutCache_; }

#ifndef NDEBUG
  std::size_t debugConstraintStackDepth() const noexcept { return traversal_.debugFrameDepth(); }
  std::size_t debugKeyPathDepth() const noexcept { return traversal_.debugKeyPathDepth(); }
  std::size_t debugSavedChildDepth() const noexcept { return traversal_.debugSavedChildDepth(); }
#endif
  LocalId peekCurrentChildLocalId() const { return currentElementKey().empty() ? LocalId::fromIndex(0) : currentElementKey().back(); }

protected:
  TextSystem& textSystem_;
  detail::TraversalContext traversal_{};
  Element const* currentElement_{nullptr};
  detail::MeasureLayoutCache* layoutCache_ = nullptr;
};

} // namespace flux
