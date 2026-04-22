#pragma once

#include <Flux/Scene/InteractionData.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/SceneNode.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstddef>
#include <memory>

namespace flux::detail::scene_builder {

struct DecorationReuse {
  std::unique_ptr<ModifierSceneNode> modifierWrapper{};
  std::unique_ptr<SceneNode> layoutWrapper{};
  std::unique_ptr<SceneNode> overlay{};
};

class EnvironmentLayerScope {
public:
  explicit EnvironmentLayerScope(EnvironmentStack& environment);
  EnvironmentLayerScope(EnvironmentStack& environment, std::span<EnvironmentLayer const> layers);
  ~EnvironmentLayerScope();

private:
  EnvironmentStack& environment_;
  std::size_t count_ = 0;
};

LayoutConstraints insetConstraints(LayoutConstraints constraints, EdgeInsets const& padding);
Point modifierOffset(detail::ElementModifiers const* mods);
bool hasPadding(detail::ElementModifiers const* mods);
bool hasOverlay(detail::ElementModifiers const* mods);
bool needsDecorationPass(detail::ElementModifiers const* mods, Size outerSize, Size contentSize,
                         bool leafOwnsPaint, InteractionData const* interaction);
DecorationReuse takeDecorationReuse(std::unique_ptr<SceneNode>& node, detail::ElementModifiers const* mods,
                                    bool leafOwnsModifierPaint, bool allowLayoutWrapperReuse);

} // namespace flux::detail::scene_builder
