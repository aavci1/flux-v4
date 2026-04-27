#pragma once

#include <Flux/Reactive/Scope.hpp>
#include <Flux/Reactive/SmallFn.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/EnvironmentBinding.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <memory>

namespace flux {

class MeasureContext;
class TextSystem;
struct Rectangle;
struct Text;
struct VStack;
struct HStack;
struct ZStack;
struct Spacer;

namespace scenegraph {
class SceneNode;
}

class MountContext {
public:
  using EnvironmentSnapshot = std::shared_ptr<std::vector<EnvironmentLayer> const>;

  MountContext(Reactive::Scope& owner, EnvironmentStack& environment, TextSystem& textSystem,
               MeasureContext& measureContext, LayoutConstraints constraints,
               LayoutHints hints = {}, Reactive::SmallFn<void()> requestRedraw = {},
               EnvironmentSnapshot environmentSnapshot = {},
               EnvironmentBinding environmentBinding = {});

  Reactive::Scope& owner() const noexcept { return *owner_; }
  EnvironmentStack& environment() const noexcept { return environment_; }
  EnvironmentBinding const& environmentBinding() const noexcept { return environmentBinding_; }
  TextSystem& textSystem() const noexcept { return textSystem_; }
  MeasureContext& measureContext() const noexcept { return measureContext_; }
  LayoutConstraints const& constraints() const noexcept { return constraints_; }
  LayoutHints const& hints() const noexcept { return hints_; }
  Reactive::SmallFn<void()> const& redrawCallback() const noexcept { return requestRedraw_; }
  EnvironmentSnapshot const& environmentSnapshot() const noexcept { return environmentSnapshot_; }

  MountContext childWithSharedScope(LayoutConstraints constraints, LayoutHints hints = {}) const;
  MountContext childWithOwnScope(LayoutConstraints constraints, LayoutHints hints = {}) const;
  MountContext childWithEnvironment(EnvironmentBinding environment, LayoutConstraints constraints,
                                    LayoutHints hints = {}) const;
  MountContext child(LayoutConstraints constraints, LayoutHints hints = {}) const = delete;
  void requestRedraw() const;

private:
  MountContext(std::shared_ptr<Reactive::Scope> owner, EnvironmentStack& environment,
               TextSystem& textSystem, MeasureContext& measureContext,
               LayoutConstraints constraints, LayoutHints hints,
               Reactive::SmallFn<void()> requestRedraw,
               EnvironmentSnapshot environmentSnapshot,
               EnvironmentBinding environmentBinding);

  std::shared_ptr<Reactive::Scope> ownedOwner_;
  Reactive::Scope* owner_;
  EnvironmentStack& environment_;
  EnvironmentBinding environmentBinding_;
  TextSystem& textSystem_;
  MeasureContext& measureContext_;
  LayoutConstraints constraints_;
  LayoutHints hints_;
  Reactive::SmallFn<void()> requestRedraw_;
  EnvironmentSnapshot environmentSnapshot_;
};

namespace detail {

std::unique_ptr<scenegraph::SceneNode> mountRectangle(Rectangle const& rectangle, MountContext& ctx);
std::unique_ptr<scenegraph::SceneNode> mountText(Text const& text, MountContext& ctx);
std::unique_ptr<scenegraph::SceneNode> mountVStack(VStack const& stack, MountContext& ctx);
std::unique_ptr<scenegraph::SceneNode> mountHStack(HStack const& stack, MountContext& ctx);
std::unique_ptr<scenegraph::SceneNode> mountZStack(ZStack const& stack, MountContext& ctx);
std::unique_ptr<scenegraph::SceneNode> mountSpacer(Spacer const& spacer, MountContext& ctx);

} // namespace detail
} // namespace flux
