#pragma once

#include <Flux/UI/Detail/TraversalContext.hpp>
#include <Flux/UI/Detail/MeasuredBuild.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>

#include "UI/SceneBuilder/MeasureLayoutCache.hpp"

#include <memory>
#include <optional>

namespace flux {

class SceneBuilder;
class TextSystem;

namespace detail {

struct SelectableTextState;

class ComponentBuildContext {
public:
  ComponentBuildContext(SceneBuilder& builder, TraversalContext const& traversal,
                        Element const& sourceElement, Element const& sceneElement, ElementType typeTag,
                        ComponentKey sceneKey, ComponentKey interactionKey,
                        ElementModifiers const* mods, bool hasOuterModifierLayers,
                        LayoutConstraints innerConstraints, Point contentOrigin, Size contentAssignedSize);

  [[nodiscard]] Element const& sourceElement() const noexcept { return sourceElement_; }
  [[nodiscard]] Element const& sceneElement() const noexcept { return sceneElement_; }
  [[nodiscard]] ElementType typeTag() const noexcept { return typeTag_; }
  [[nodiscard]] ComponentKey const& key() const noexcept { return sceneKey_; }
  [[nodiscard]] ComponentKey const& interactionKey() const noexcept { return interactionKey_; }
  [[nodiscard]] LayoutConstraints const& constraints() const noexcept { return traversal_.frame().constraints; }
  [[nodiscard]] LayoutHints const& hints() const noexcept { return traversal_.frame().hints; }
  [[nodiscard]] Point origin() const noexcept { return traversal_.frame().origin; }
  [[nodiscard]] Size assignedSize() const noexcept { return traversal_.frame().assignedSize; }
  [[nodiscard]] bool hasAssignedWidth() const noexcept { return traversal_.frame().hasAssignedWidth; }
  [[nodiscard]] bool hasAssignedHeight() const noexcept { return traversal_.frame().hasAssignedHeight; }
  [[nodiscard]] ElementModifiers const* modifiers() const noexcept { return mods_; }
  [[nodiscard]] LayoutConstraints const& innerConstraints() const noexcept { return innerConstraints_; }
  [[nodiscard]] Point contentOrigin() const noexcept { return contentOrigin_; }
  [[nodiscard]] Size contentAssignedSize() const noexcept { return contentAssignedSize_; }

  [[nodiscard]] TextSystem& textSystem() const;
  [[nodiscard]] EnvironmentStack& environment() const;
  [[nodiscard]] Theme const& theme() const;
  [[nodiscard]] MeasureLayoutCache* measureLayoutCache() const noexcept;
  [[nodiscard]] MeasureLayoutKey makeMeasureLayoutKey(LayoutConstraints const& constraints,
                                                      LayoutHints const& hints) const;

  [[nodiscard]] ComponentKey childKey(LocalId local) const;
  [[nodiscard]] Size measureElement(Element const& element, ComponentKey const& key,
                                    LayoutConstraints const& constraints, LayoutHints const& hints) const;
  [[nodiscard]] Size measureChild(Element const& element, LocalId local,
                                  LayoutConstraints const& constraints, LayoutHints const& hints) const;
  void recordChildMeasure(Element const& child, ComponentKey const& childKey,
                          LayoutConstraints const& constraints, LayoutHints const& hints, Size size) const;
  void recordMeasuredSize(Element const& child, LocalId local, LayoutConstraints const& constraints,
                          LayoutHints const& hints, Size size) const;
  [[nodiscard]] std::unique_ptr<scenegraph::SceneNode>
  buildChild(Element const& child, LocalId local, LayoutConstraints const& constraints,
             LayoutHints const& hints, Point origin, Size assignedSize, bool hasAssignedWidth,
             bool hasAssignedHeight,
             std::unique_ptr<scenegraph::SceneNode> existing = nullptr) const;

  [[nodiscard]] Size outerSize() const;
  [[nodiscard]] Size layoutOuterSize() const;
  [[nodiscard]] Size paddedContentSize() const;
  [[nodiscard]] LayoutConstraints contentBoxConstraints() const;
  void finalizeOuterSizes(Size contentSize, Size& outerSize, Size& layoutOuterSize) const;

  [[nodiscard]] std::unique_ptr<scenegraph::InteractionData> makeInteractionData() const;
  [[nodiscard]] std::unique_ptr<scenegraph::InteractionData>
  makeSelectableTextInteraction(std::shared_ptr<SelectableTextState> const& state) const;
  [[nodiscard]] ComponentBuildResult
  buildFallback(std::unique_ptr<scenegraph::SceneNode> existing) const;

private:
  SceneBuilder& builder_;
  TraversalContext const& traversal_;
  Element const& sourceElement_;
  Element const& sceneElement_;
  ElementType typeTag_ = ElementType::Unknown;
  ComponentKey sceneKey_{};
  ComponentKey interactionKey_{};
  ElementModifiers const* mods_ = nullptr;
  bool hasOuterModifierLayers_ = false;
  LayoutConstraints innerConstraints_{};
  Point contentOrigin_{};
  Size contentAssignedSize_{};
  mutable std::optional<Size> outerSize_{};
  mutable std::optional<Size> layoutOuterSize_{};
  mutable std::optional<Size> paddedContentSize_{};
  mutable std::optional<LayoutConstraints> contentBoxConstraints_{};
};

} // namespace detail

} // namespace flux
