#pragma once

#include <Flux/Reactive2/Scope.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <functional>
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
  MountContext(Reactive2::Scope& owner, EnvironmentStack& environment, TextSystem& textSystem,
               MeasureContext& measureContext, LayoutConstraints constraints,
               LayoutHints hints = {}, std::function<void()> requestRedraw = {});

  Reactive2::Scope& owner() const noexcept { return owner_; }
  EnvironmentStack& environment() const noexcept { return environment_; }
  TextSystem& textSystem() const noexcept { return textSystem_; }
  MeasureContext& measureContext() const noexcept { return measureContext_; }
  LayoutConstraints const& constraints() const noexcept { return constraints_; }
  LayoutHints const& hints() const noexcept { return hints_; }
  std::function<void()> const& redrawCallback() const noexcept { return requestRedraw_; }

  MountContext child(LayoutConstraints constraints, LayoutHints hints = {}) const;
  void requestRedraw() const;

private:
  Reactive2::Scope& owner_;
  EnvironmentStack& environment_;
  TextSystem& textSystem_;
  MeasureContext& measureContext_;
  LayoutConstraints constraints_;
  LayoutHints hints_;
  std::function<void()> requestRedraw_;
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
