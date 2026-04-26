#pragma once

#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Types.hpp>

#include <memory>
#include <optional>
#include <string>

namespace flux {

struct RootHolder;
class Window;

class Runtime {
public:
  explicit Runtime(Window& window);
  ~Runtime();

  Runtime(Runtime const&) = delete;
  Runtime& operator=(Runtime const&) = delete;

  void setRoot(std::unique_ptr<RootHolder> holder);
  void handleInput(InputEvent const& event);
  void beginShutdown();

  bool wantsTextInput() const noexcept { return true; }
  bool textCacheOverlayEnabled() const noexcept { return false; }
  bool isActionCurrentlyEnabled(std::string const& name) const;
  Window& window() noexcept;
  Window const& window() const noexcept;

  static Runtime* current() noexcept;

  std::optional<Rect> layoutRectForKey(ComponentKey const& key) const;
  std::optional<Rect> layoutRectForLeafKeyPrefix(ComponentKey const& key) const;

private:
  static thread_local Runtime* current_;

  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
