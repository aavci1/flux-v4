#include "Core/PlatformApplication.hpp"

#include <cctype>
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

std::string sanitizeAppName(std::string name) {
  std::string out;
  out.reserve(name.size());
  for (unsigned char c : name) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.') {
      out.push_back(static_cast<char>(c));
    } else if (c == ' ') {
      out.push_back('-');
    }
  }
  return out.empty() ? "flux" : out;
}

std::string appDir(std::string const& base, std::string const& appName) {
  std::filesystem::path path = std::filesystem::path(base) / "flux" / sanitizeAppName(appName);
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return path.string();
}

class WaylandApplication final : public PlatformApplication {
public:
  void initialize() override {}

  void setApplicationName(std::string name) override {
    appName_ = sanitizeAppName(std::move(name));
  }

  std::string applicationName() const override {
    return appName_.empty() ? "flux" : appName_;
  }

  void setMenuBar(MenuBar const& menu, MenuActionDispatcher dispatcher) override {
    claimedShortcuts_.clear();
    collectShortcuts(menu);
    dispatcher_ = std::move(dispatcher);
  }

  void setTerminateHandler(std::function<void()> handler) override {
    terminateHandler_ = std::move(handler);
  }

  std::unordered_set<ShortcutKey, ShortcutKeyHash> menuClaimedShortcuts() const override {
    return claimedShortcuts_;
  }

  void revalidateMenuItems(std::function<bool(std::string const&)>) override {}

  std::string userDataDir() const override {
    return appDir(envOr("XDG_DATA_HOME", envOr("HOME", ".") + "/.local/share"), applicationName());
  }

  std::string cacheDir() const override {
    return appDir(envOr("XDG_CACHE_HOME", envOr("HOME", ".") + "/.cache"), applicationName());
  }

private:
  void collectShortcuts(MenuItem const& item) {
    if (!item.actionName.empty() && (item.shortcut.key != 0 || item.shortcut.modifiers != Modifiers::None)) {
      claimedShortcuts_.insert(ShortcutKey{.key = item.shortcut.key, .modifiers = item.shortcut.modifiers});
    }
    for (MenuItem const& child : item.children) {
      collectShortcuts(child);
    }
  }

  void collectShortcuts(MenuBar const& menu) {
    for (MenuItem const& item : menu.menus) {
      collectShortcuts(item);
    }
  }

  MenuActionDispatcher dispatcher_;
  std::function<void()> terminateHandler_;
  std::unordered_set<ShortcutKey, ShortcutKeyHash> claimedShortcuts_;
  std::string appName_ = "flux";
};

} // namespace

namespace detail {

std::unique_ptr<PlatformApplication> createPlatformApplication() {
  return std::make_unique<WaylandApplication>();
}

} // namespace detail
} // namespace flux
