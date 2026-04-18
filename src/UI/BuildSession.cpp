#include "UI/BuildSession.hpp"

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/StateStore.hpp>

namespace flux {

ResolvedRootScene resolveOverlayRootScene(std::optional<Element> const& content, ComponentKey rootKey) {
  return ResolvedRootScene{
      .element = content ? &*content : nullptr,
      .rootKey = std::move(rootKey),
      .descendantsStable = false,
  };
}

BuildSession::BuildSession(TextSystem& textSystem, EnvironmentStack& environment,
                           EnvironmentLayer windowEnvironment, SceneGeometryIndex* geometryIndex)
    : textSystem_(textSystem)
    , environment_(environment)
    , windowEnvironment_(std::move(windowEnvironment))
    , geometryIndex_(geometryIndex) {}

std::unique_ptr<SceneNode> BuildSession::buildRoot(ResolvedRootScene const& resolved, NodeId id,
                                                   LayoutConstraints const& constraints,
                                                   std::unique_ptr<SceneNode> existing) const {
  if (!resolved.element) {
    if (geometryIndex_) {
      geometryIndex_->clear();
    }
    return nullptr;
  }

  SceneBuilder sceneBuilder{textSystem_, environment_, geometryIndex_};
  environment_.push(windowEnvironment_);

  struct EnvironmentPop {
    EnvironmentStack& environment;
    ~EnvironmentPop() { environment.pop(); }
  } environmentPop{environment_};

  StateStore* const store = StateStore::current();
  if (store) {
    store->pushCompositePathStable(resolved.descendantsStable);
  }
  struct CompositeStablePop {
    StateStore* store = nullptr;
    ~CompositeStablePop() {
      if (store) {
        store->popCompositePathStable();
      }
    }
  } stablePop{store};

  return sceneBuilder.build(*resolved.element, id, constraints, std::move(existing), resolved.rootKey);
}

} // namespace flux
