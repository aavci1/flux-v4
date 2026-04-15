#pragma once

/// \file Flux/UI/LayoutContext.hpp
///
/// Context for \ref Element::measure and \ref Element::layout (no SceneGraph / EventMap).

#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>

#include <cstddef>
#include <memory>
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

class LayoutContext {
public:
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

  ComponentKey nextCompositeKey();

  void advanceChildSlot();

  ComponentKey leafComponentKey() const;

  void rewindChildKeyIndex();

  void beginCompositeBodySubtree(ComponentKey compositeKey);
  bool consumeCompositeBodySubtreeRootSkip();

  void pushCompositeKeyTail(ComponentKey const& compositeKey);
  void popCompositeKeyTail();

  void registerCompositeSubtreeRootIfPending(LayoutNodeId layoutNodeId);

  std::unordered_map<ComponentKey, LayoutNodeId, ComponentKeyHash> const& subtreeRootLayouts() const;

  void pushActiveElementModifiers(ElementModifiers const* m);
  void popActiveElementModifiers();
  ElementModifiers const* activeElementModifiers() const noexcept;

  /// Layer-local → window transform stack (mirrors SceneGraph layer stack during layout).
  void pushLayerWorldTransform(Mat3 const& localToParentLayer);
  void popLayerWorldTransform();

  [[nodiscard]] Mat3 currentLayerWorldTransform() const;

  void pushLayoutParent(LayoutNodeId id);
  void popLayoutParent();
  [[nodiscard]] LayoutNodeId currentLayoutParent() const;

  void setCurrentElement(Element const* el) noexcept { currentElement_ = el; }
  [[nodiscard]] Element const* currentElement() const noexcept { return currentElement_; }

  Element* findPinnedCompositeBody(ComponentKey const& key) const;
  Element& pinCompositeBody(ComponentKey key, Element&& el);

  /// Move a subtree root \c Element into frame-local storage so \p LayoutNode::element pointers
  /// remain valid through the render phase (stack temporaries are not safe across layout → render).
  Element& pinElement(Element&& el);

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
  friend class OverlayManager;
  friend struct LayoutContextTestAccess;

  LayoutContext(TextSystem& ts, LayoutEngine& layout, LayoutTree& tree, MeasureCache* measureCache = nullptr);
  ~LayoutContext();

  TextSystem& textSystem_;
  LayoutEngine& layoutEngine_;
  LayoutTree& tree_;
  struct LayoutFrame {
    LayoutConstraints constraints{};
    LayoutHints hints{};
  };
  std::vector<LayoutFrame> layoutStack_;

  std::vector<std::size_t> keyStack_;
  std::vector<std::size_t> savedChildIndices_;
  std::size_t nextChildIndex_{0};
  bool skipNextLayoutChildAdvance_{false};
  bool pendingCompositeSubtreeRoot_{false};
  ComponentKey pendingCompositeSubtreeKey_{};
  std::unordered_map<ComponentKey, LayoutNodeId, ComponentKeyHash> subtreeRootLayouts_{};
  std::unordered_map<ComponentKey, Element*, ComponentKeyHash> compositeBodyPins_{};
  MeasureCache* measureCache_{nullptr};
  std::vector<ElementModifiers const*> activeElementModifiers_{};

  std::vector<Mat3> layerWorldStack_{};
  std::vector<LayoutNodeId> layoutParentStack_{};
  Element const* currentElement_{nullptr};

  std::unique_ptr<detail::ElementPinStorage> elementPins_{};
};

} // namespace flux
