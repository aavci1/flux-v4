#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Reactive/Scope.hpp>
#include <Flux/Reactive/SmallFn.hpp>
#include <Flux/UI/EnvironmentBinding.hpp>

#include <memory>

namespace flux {

struct RootHolder;
class TextSystem;

namespace scenegraph {
class SceneGraph;
class SceneNode;
}

class MountRoot {
public:
  MountRoot(std::unique_ptr<RootHolder> root, TextSystem& textSystem,
            EnvironmentBinding environment, Size viewportSize,
            Reactive::SmallFn<void()> requestRedraw = {});
  ~MountRoot();

  MountRoot(MountRoot const&) = delete;
  MountRoot& operator=(MountRoot const&) = delete;

  void mount(scenegraph::SceneGraph& sceneGraph);
  void unmount(scenegraph::SceneGraph& sceneGraph);
  void resize(Size viewportSize, scenegraph::SceneGraph& sceneGraph);

  bool mounted() const noexcept { return mounted_; }

private:
  std::unique_ptr<RootHolder> root_;
  TextSystem& textSystem_;
  EnvironmentBinding environment_;
  Size viewportSize_{};
  Reactive::SmallFn<void()> requestRedraw_;
  Reactive::Scope rootScope_{};
  bool mounted_ = false;
};

} // namespace flux
