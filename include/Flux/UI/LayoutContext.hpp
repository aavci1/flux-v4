#pragma once

/// \file Flux/UI/LayoutContext.hpp
///
/// Context for \ref Element::measure and \ref Element::layout during retained-scene rebuilds.

#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/LayoutTree.hpp>

#include <memory>
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
class SceneBuilder;

class LayoutContext : public MeasureContext {
public:
  LayoutEngine& layoutEngine();

  LayoutTree& tree();
  LayoutTree const& tree() const;

  void registerCompositeSubtreeRootIfPending(LayoutNodeId layoutNodeId);

  /// Layer-local → window transform stack used while building layout/world geometry.
  void pushLayerWorldTransform(Mat3 const& localToParentLayer);
  void popLayerWorldTransform();

  [[nodiscard]] Mat3 currentLayerWorldTransform() const;

  void pushLayoutParent(LayoutNodeId id);
  void popLayoutParent();
  [[nodiscard]] LayoutNodeId currentLayoutParent() const;

  /// Move a subtree root \c Element into frame-local storage so \p LayoutNode::element pointers
  /// remain valid through the current rebuild (stack temporaries are not safe across layout → scene build).
  Element& pinElement(Element&& el);
  [[nodiscard]] std::shared_ptr<detail::ElementPinStorage> pinnedElements() const noexcept;

  /// Append a completed layout node as a child of \ref currentLayoutParent (or root).
  LayoutNodeId pushLayoutNode(LayoutNode&& node);

#ifndef NDEBUG
  std::size_t debugLayerWorldStackDepth() const noexcept { return layerWorldStack_.size(); }
#endif

private:
  friend class Runtime;
  friend class BuildOrchestrator;
  friend class SceneBuilder;
  friend class OverlayManager;
  friend struct LayoutContextTestAccess;

  LayoutContext(TextSystem& ts, LayoutEngine& layout, LayoutTree& tree, MeasureCache* measureCache = nullptr);
  ~LayoutContext();

  LayoutEngine& layoutEngine_;
  LayoutTree& tree_;

  std::vector<Mat3> layerWorldStack_{};
  std::vector<LayoutNodeId> layoutParentStack_{};

  std::shared_ptr<detail::ElementPinStorage> elementPins_{};
};

} // namespace flux
