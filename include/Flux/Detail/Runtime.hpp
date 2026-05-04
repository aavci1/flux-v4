#pragma once

#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/UI/ActionRegistry.hpp>

#include <memory>
#include <optional>
#include <string>

namespace flux {

struct RootHolder;
class Window;
namespace scenegraph {
class SceneGraph;
}

class Runtime {
public:
  explicit Runtime(Window& window);
  ~Runtime();

  Runtime(Runtime const&) = delete;
  Runtime& operator=(Runtime const&) = delete;

  void setRoot(std::unique_ptr<RootHolder> holder);
  void handleInput(InputEvent const& event);
  void handleWindowEvent(WindowEvent const& event);
  void beginShutdown();
  void beginShutdown(scenegraph::SceneGraph* sceneGraph);

  bool wantsTextInput() const noexcept { return true; }
  bool textCacheOverlayEnabled() const noexcept { return false; }
  bool isActionCurrentlyEnabled(std::string const& name) const;
  bool dispatchAction(std::string const& name);
  ActionRegistry& actionRegistry() noexcept;
  ActionRegistry const& actionRegistry() const noexcept;
  Window& window() noexcept;
  Window const& window() const noexcept;

  static Runtime* current() noexcept;

private:
  static thread_local Runtime* current_;

  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
