#include "Core/PlatformApplication.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>

namespace flux {
namespace {

std::string envOr(std::string const& name, std::string fallback) {
  if (char const* value = std::getenv(name.c_str())) {
    if (*value) return value;
  }
  return fallback;
}

std::string appDir(std::string const& base) {
  std::filesystem::path path = std::filesystem::path(base) / "flux" / "Solitaire";
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return path.string();
}

class WaylandApplication final : public PlatformApplication {
public:
  void initialize() override {}

  void setMenuBar(MenuBar const&, MenuActionDispatcher dispatcher) override {
    dispatcher_ = std::move(dispatcher);
  }

  void setTerminateHandler(std::function<void()> handler) override {
    terminateHandler_ = std::move(handler);
  }

  std::unordered_set<ShortcutKey, ShortcutKeyHash> menuClaimedShortcuts() const override {
    return {};
  }

  void revalidateMenuItems(std::function<bool(std::string const&)>) override {}

  std::string userDataDir() const override {
    return appDir(envOr("XDG_DATA_HOME", envOr("HOME", ".") + "/.local/share"));
  }

  std::string cacheDir() const override {
    return appDir(envOr("XDG_CACHE_HOME", envOr("HOME", ".") + "/.cache"));
  }

private:
  MenuActionDispatcher dispatcher_;
  std::function<void()> terminateHandler_;
};

} // namespace

namespace detail {

std::unique_ptr<PlatformApplication> createPlatformApplication() {
  return std::make_unique<WaylandApplication>();
}

} // namespace detail
} // namespace flux
