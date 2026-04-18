#pragma once

/// \file Flux/UI/LayoutContext.hpp
///
/// Context for \ref Element::measure and \ref Element::layout during retained-scene rebuilds.

#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace flux {

namespace detail {
struct ElementPinStorage;
}

struct LayoutContextTestAccess;

class TextSystem;
class Runtime;
class BuildOrchestrator;
class MeasureCache;
struct ElementModifiers;
class Element;
class SceneBuilder;

class LayoutContext {
public:
  struct SubtreeRootRecord {
    LayoutNodeId rootId{};
    std::uint64_t lastVisitedEpoch = 0;
  };
  using SubtreeRootMap = std::unordered_map<ComponentKey, SubtreeRootRecord, ComponentKeyHash>;

  TextSystem& textSystem();
  LayoutEngine& layoutEngine();
  MeasureCache* measureCache() const;

  LayoutTree& tree();
  LayoutTree const& tree() const;

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

  void beginCompositeBodySubtree(ComponentKey compositeKey);
  bool consumeCompositeBodySubtreeRootSkip();

  void pushCompositeKeyTail(ComponentKey const& compositeKey);
  void popCompositeKeyTail();

  void registerCompositeSubtreeRootIfPending(LayoutNodeId layoutNodeId);

  SubtreeRootMap const& subtreeRootLayouts() const;
  bool canReuseRetainedCompositeSubtree(ComponentKey const& compositeKey, Rect const& assignedFrame,
                                        LayoutConstraints const& constraints, LayoutHints const& hints) const;
  bool canReuseRetainedCompositeSubtree(LayoutNodeId rootId, Rect const& assignedFrame,
                                        LayoutConstraints const& constraints, LayoutHints const& hints) const;
  bool reuseRetainedCompositeSubtree(ComponentKey const& compositeKey, Rect const& assignedFrame);
  bool reuseRetainedCompositeSubtree(ComponentKey const& compositeKey, LayoutNodeId rootId,
                                     Rect const& assignedFrame);

  void pushActiveElementModifiers(ElementModifiers const* m);
  void popActiveElementModifiers();
  ElementModifiers const* activeElementModifiers() const noexcept;

  /// Layer-local → window transform stack used while building layout/world geometry.
  void pushLayerWorldTransform(Mat3 const& localToParentLayer);
  void popLayerWorldTransform();

  [[nodiscard]] Mat3 currentLayerWorldTransform() const;

  void pushLayoutParent(LayoutNodeId id);
  void popLayoutParent();
  [[nodiscard]] LayoutNodeId currentLayoutParent() const;

  void setCurrentElement(Element const* el) noexcept { currentElement_ = el; }
  [[nodiscard]] Element const* currentElement() const noexcept { return currentElement_; }

  /// Move a subtree root \c Element into frame-local storage so \p LayoutNode::element pointers
  /// remain valid through the current rebuild (stack temporaries are not safe across layout → scene build).
  Element& pinElement(Element&& el);
  [[nodiscard]] std::shared_ptr<detail::ElementPinStorage> pinnedElements() const noexcept;

  /// Append a completed layout node as a child of \ref currentLayoutParent (or root).
  LayoutNodeId pushLayoutNode(LayoutNode&& node);

#ifndef NDEBUG
  std::size_t debugLayerWorldStackDepth() const noexcept { return layerWorldStack_.size(); }
  std::size_t debugConstraintStackDepth() const noexcept { return layoutStack_.size(); }
  std::size_t debugKeyPathDepth() const noexcept { return keyStack_.size(); }
  std::size_t debugSavedChildDepth() const noexcept { return savedChildIndices_.size(); }
#endif

private:
  friend class Runtime;
  friend class BuildOrchestrator;
  friend class SceneBuilder;
  friend class OverlayManager;
  friend struct LayoutContextTestAccess;

  LayoutContext(TextSystem& ts, LayoutEngine& layout, LayoutTree& tree, MeasureCache* measureCache = nullptr,
                SubtreeRootMap* retainedRoots = nullptr, std::uint64_t subtreeRootEpoch = 0);
  ~LayoutContext();

  TextSystem& textSystem_;
  LayoutEngine& layoutEngine_;
  LayoutTree& tree_;
  struct LayoutFrame {
    LayoutConstraints constraints{};
    LayoutHints hints{};
  };
  std::vector<LayoutFrame> layoutStack_;

  std::vector<LocalId> keyStack_;
  std::vector<std::optional<LocalId>> explicitChildLocalIdStack_;
  std::vector<std::size_t> savedChildIndices_;
  std::size_t nextChildIndex_{0};
  bool skipNextLayoutChildAdvance_{false};
  bool pendingCompositeSubtreeRoot_{false};
  ComponentKey pendingCompositeSubtreeKey_{};
  SubtreeRootMap ownedSubtreeRoots_{};
  SubtreeRootMap* subtreeRoots_{nullptr};
  std::uint64_t subtreeRootEpoch_{0};
  MeasureCache* measureCache_{nullptr};
  std::vector<ElementModifiers const*> activeElementModifiers_{};

  std::vector<Mat3> layerWorldStack_{};
  std::vector<LayoutNodeId> layoutParentStack_{};
  Element const* currentElement_{nullptr};

  std::shared_ptr<detail::ElementPinStorage> elementPins_{};

  [[nodiscard]] LocalId currentChildLocalId() const;
};

} // namespace flux
