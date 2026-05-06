#pragma once

#include <Flux/Core/MenuItem.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

namespace flux {

struct ShortcutKey {
  KeyCode key = 0;
  Modifiers modifiers = Modifiers::None;

  bool operator==(ShortcutKey const&) const = default;
};

struct ShortcutKeyHash {
  std::size_t operator()(ShortcutKey const& value) const noexcept {
    return (static_cast<std::size_t>(value.key) << 32u) ^
           static_cast<std::size_t>(value.modifiers);
  }
};

using MenuActionDispatcher = std::function<bool(std::string const&)>;

class PlatformApplication {
public:
  virtual ~PlatformApplication() = default;

  virtual void initialize() = 0;
  virtual void setApplicationName(std::string name) = 0;
  virtual std::string applicationName() const = 0;
  virtual void setMenuBar(MenuBar const& menu, MenuActionDispatcher dispatcher) = 0;
  virtual void setTerminateHandler(std::function<void()> handler) = 0;
  virtual void requestTerminate() = 0;
  virtual std::unordered_set<ShortcutKey, ShortcutKeyHash> menuClaimedShortcuts() const = 0;
  virtual void revalidateMenuItems(std::function<bool(std::string const&)> isEnabled) = 0;
  virtual std::string userDataDir() const = 0;
  virtual std::string cacheDir() const = 0;
};

namespace detail {
std::unique_ptr<PlatformApplication> createPlatformApplication();
}

} // namespace flux
