#pragma once

/// \file Flux/UI/SceneBuilder.hpp
///
/// Part of the Flux public API.

#include <Flux/Scene/SceneNode.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Detail/TraversalContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/SceneGeometryIndex.hpp>

#include <memory>
#include <span>
#include <vector>

namespace flux {

class TextSystem;
class ModifierSceneNode;
namespace detail {
class MeasureLayoutCache;
class ComponentBuildContext;
}

class SceneBuilder {
public:
  struct BuildStats {
    std::size_t resolvedNodes = 0;
    std::size_t measuredNodes = 0;
    std::size_t arrangedNodes = 0;
    std::size_t materializedNodes = 0;
    std::size_t skippedNodes = 0;
  };

  SceneBuilder(TextSystem& textSystem, EnvironmentStack& environment,
               SceneGeometryIndex* geometryIndex = nullptr);
  ~SceneBuilder();

  std::unique_ptr<SceneNode> build(Element const& el, NodeId id, LayoutConstraints const& constraints,
                                   std::unique_ptr<SceneNode> existing = nullptr,
                                   ComponentKey rootKey = {});

  std::unique_ptr<SceneNode> build(Element const& el, NodeId id, LayoutConstraints const& constraints,
                                   std::unique_ptr<SceneNode> existing, ComponentKey rootKey,
                                   bool rootUsesMaxWidthAsAssigned,
                                   bool rootUsesMaxHeightAsAssigned);

  std::unique_ptr<SceneNode> buildOrReuse(Element const& el, NodeId id, std::unique_ptr<SceneNode> existing);

  void reconcileChildren(SceneNode& parent, std::span<Element const> newChildren,
                         std::vector<std::unique_ptr<SceneNode>>& existingChildren);
  [[nodiscard]] BuildStats const& lastBuildStats() const noexcept { return lastBuildStats_; }

private:
  friend class detail::ComponentBuildContext;

  TextSystem& textSystem_;
  EnvironmentStack& environment_;
  SceneGeometryIndex* geometryIndex_ = nullptr;
  mutable std::unique_ptr<detail::MeasureLayoutCache> measureLayoutCache_{};
  detail::TraversalContext traversal_{};
  std::size_t buildFrameDepth_ = 0;
  mutable BuildStats lastBuildStats_{};

  [[nodiscard]] detail::TraversalContext::Frame const& frame() const;
  void pushFrame(LayoutConstraints const& constraints, LayoutHints const& hints, Point origin,
                 ComponentKey key, Size assignedSize = {}, bool hasAssignedWidth = false,
                 bool hasAssignedHeight = false);
  void popFrame();
  [[nodiscard]] bool canRetainExistingSubtree(detail::ResolvedElement const& resolved,
                                              SceneNode const& existing) const;
  void stampRetainedBuild(SceneNode& node, detail::ResolvedElement const& resolved) const;

  [[nodiscard]] Size measureElement(Element const& el, LayoutConstraints const& constraints,
                                    LayoutHints const& hints, ComponentKey const& key) const;

  std::unique_ptr<SceneNode> decorateNode(std::unique_ptr<SceneNode> root, bool leafOwnsModifierPaint,
                                          detail::ElementModifiers const* mods,
                                          std::unique_ptr<ModifierSceneNode> existingModifierWrapper,
                                          std::unique_ptr<SceneNode> existingLayoutWrapper,
                                          std::unique_ptr<SceneNode> existingOverlay,
                                          Size layoutOuterSize, Size outerSize, Point subtreeOffset,
                                          EdgeInsets const& padding, std::unique_ptr<InteractionData> interaction);

  std::unique_ptr<SceneNode> buildResolved(Element const& el, detail::ResolvedElement const& resolved,
                                           NodeId id, std::unique_ptr<SceneNode> existing);
};

} // namespace flux
