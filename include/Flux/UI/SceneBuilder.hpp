#pragma once

/// \file Flux/UI/SceneBuilder.hpp
///
/// Scenegraph-native UI builder.

#include <Flux/Core/ComponentKey.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/UI/Detail/TraversalContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <memory>

namespace flux {

class TextSystem;
class MeasureContext;
namespace scenegraph {
class SceneGraph;
}
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
               scenegraph::SceneGraph* sceneGraph = nullptr);
  ~SceneBuilder();

  std::unique_ptr<scenegraph::SceneNode> build(Element const& el, LayoutConstraints const& constraints,
                                               ComponentKey rootKey = {},
                                               bool rootUsesMaxWidthAsAssigned = true,
                                               bool rootUsesMaxHeightAsAssigned = true);
  std::unique_ptr<scenegraph::SceneNode> buildSubtree(Element const& el,
                                                      LayoutConstraints const& constraints,
                                                      LayoutHints const& hints, Point origin,
                                                      ComponentKey key, Size assignedSize,
                                                      bool hasAssignedWidth,
                                                      bool hasAssignedHeight,
                                                      std::unique_ptr<scenegraph::SceneNode> existing = nullptr);

  std::unique_ptr<scenegraph::SceneNode>
  buildOrReuse(Element const& el, std::unique_ptr<scenegraph::SceneNode> existing = nullptr);

  [[nodiscard]] BuildStats const& lastBuildStats() const noexcept { return lastBuildStats_; }

private:
  friend class detail::ComponentBuildContext;

  TextSystem& textSystem_;
  EnvironmentStack& environment_;
  scenegraph::SceneGraph* sceneGraph_ = nullptr;
  mutable std::unique_ptr<detail::MeasureLayoutCache> measureLayoutCache_{};
  mutable std::unique_ptr<MeasureContext> measureContext_{};
  detail::TraversalContext traversal_{};
  std::size_t buildFrameDepth_ = 0;
  mutable BuildStats lastBuildStats_{};

  [[nodiscard]] detail::TraversalContext::Frame const& frame() const;
  void pushFrame(LayoutConstraints const& constraints, LayoutHints const& hints, Point origin,
                 ComponentKey key, Size assignedSize = {}, bool hasAssignedWidth = false,
                 bool hasAssignedHeight = false);
  void popFrame();

  [[nodiscard]] Size measureElement(Element const& el, LayoutConstraints const& constraints,
                                    LayoutHints const& hints, ComponentKey const& key) const;
  std::unique_ptr<scenegraph::SceneNode>
  wrapModifierLayer(std::unique_ptr<scenegraph::SceneNode> root,
                    detail::ElementModifiers const& layer, ComponentKey const& componentKey,
                    ComponentKey const& interactionKey,
                    LayoutConstraints const& constraints, LayoutHints const& hints, Point origin,
                    Size innerSize, Size outerSize, bool applyBoxPaint,
                    std::unique_ptr<scenegraph::SceneNode> existingWrapper = nullptr);

  std::unique_ptr<scenegraph::SceneNode> buildResolved(Element const& el,
                                                       detail::ResolvedElement const& resolved,
                                                       std::unique_ptr<scenegraph::SceneNode> existing);
};

} // namespace flux
