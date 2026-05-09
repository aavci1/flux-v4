#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

#include "Core/PlatformApplication.hpp"
#include "Platform/Linux/WaylandNativeSurface.hpp"
#include "Platform/Linux/WaylandOutputs.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace flux {
namespace {

void vkCheck(VkResult result, char const* what) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::string(what) + " failed");
  }
}

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

  void requestTerminate() override {
    if (terminateHandler_) {
      terminateHandler_();
    }
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

  std::vector<std::string> availableOutputs() const override {
    return linux_platform::availableWaylandOutputs();
  }

  std::span<char const* const> requiredVulkanInstanceExtensions() const override {
    static constexpr char const* exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
    };
    return exts;
  }

  VkSurfaceKHR createVulkanSurface(VkInstance instance, void* nativeHandle) override {
    auto* native = static_cast<WaylandNativeSurface*>(nativeHandle);
    if (!native || !native->display || !native->surface) {
      throw std::runtime_error("Invalid Wayland Vulkan surface handle");
    }
    VkWaylandSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
    info.display = native->display;
    info.surface = native->surface;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    vkCheck(vkCreateWaylandSurfaceKHR(instance, &info, nullptr, &surface), "vkCreateWaylandSurfaceKHR");
    return surface;
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
