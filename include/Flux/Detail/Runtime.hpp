#pragma once

#include <Flux/Core/ComponentKey.hpp>
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
  void beginShutdown();

  bool wantsTextInput() const noexcept { return false; }
  bool textCacheOverlayEnabled() const noexcept { return false; }
  bool isActionCurrentlyEnabled(std::string const& name) const;

  std::optional<Rect> layoutRectForKey(ComponentKey const& key) const;
  std::optional<Rect> layoutRectForLeafKeyPrefix(ComponentKey const& key) const;

private:
  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
