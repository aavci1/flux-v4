#include "Platform/Linux/KmsPlatform.hpp"

#include "Core/PlatformWindowCreate.hpp"

#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <xf86drm.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace flux {
namespace {

KmsApplication* gKmsApplication = nullptr;

struct TerminationSignalState {
  struct sigaction previousSigInt {};
  struct sigaction previousSigTerm {};
  bool sigIntInstalled = false;
  bool sigTermInstalled = false;
};

TerminationSignalState gTerminationSignals;
volatile sig_atomic_t gTerminateSignalPending = 0;
volatile sig_atomic_t gSignalWakeFd = -1;

void terminateSignalHandler(int) {
  gTerminateSignalPending = 1;
  int const wakeFd = static_cast<int>(gSignalWakeFd);
  if (wakeFd >= 0) {
    char const c = 1;
    (void)write(wakeFd, &c, 1);
  }
}

void restoreTerminationSignalHandlers() {
  if (gTerminationSignals.sigIntInstalled) {
    sigaction(SIGINT, &gTerminationSignals.previousSigInt, nullptr);
    gTerminationSignals.sigIntInstalled = false;
  }
  if (gTerminationSignals.sigTermInstalled) {
    sigaction(SIGTERM, &gTerminationSignals.previousSigTerm, nullptr);
    gTerminationSignals.sigTermInstalled = false;
  }
  gSignalWakeFd = -1;
  gTerminateSignalPending = 0;
}

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
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.') out.push_back(static_cast<char>(c));
    else if (c == ' ') out.push_back('-');
  }
  return out.empty() ? "flux" : out;
}

std::string appDir(std::string const& base, std::string const& appName) {
  std::filesystem::path path = std::filesystem::path(base) / "flux" / sanitizeAppName(appName);
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return path.string();
}

int openRestricted(char const* path, int flags, void*) {
  return open(path, flags | O_CLOEXEC);
}

void closeRestricted(int fd, void*) {
  close(fd);
}

libinput_interface const kLibinputInterface{openRestricted, closeRestricted};

std::string connectorName(drmModeConnector const& connector) {
  char const* type = "UNKNOWN";
  switch (connector.connector_type) {
  case DRM_MODE_CONNECTOR_HDMIA: type = "HDMI-A"; break;
  case DRM_MODE_CONNECTOR_HDMIB: type = "HDMI-B"; break;
  case DRM_MODE_CONNECTOR_eDP: type = "eDP"; break;
  case DRM_MODE_CONNECTOR_DisplayPort: type = "DP"; break;
  case DRM_MODE_CONNECTOR_DVID: type = "DVI-D"; break;
  case DRM_MODE_CONNECTOR_DVII: type = "DVI-I"; break;
  case DRM_MODE_CONNECTOR_VGA: type = "VGA"; break;
  default: break;
  }
  return std::string(type) + "-" + std::to_string(connector.connector_type_id);
}

drmModeModeInfo chooseMode(drmModeConnector const& connector) {
  if (connector.count_modes <= 0) {
    throw std::runtime_error("Connected DRM output has no display modes");
  }
  for (int i = 0; i < connector.count_modes; ++i) {
    if ((connector.modes[i].type & DRM_MODE_TYPE_PREFERRED) != 0) return connector.modes[i];
  }
  return connector.modes[0];
}

std::uint32_t refreshRateMilliHz(drmModeModeInfo const& mode) {
  if (mode.vrefresh > 0) return static_cast<std::uint32_t>(mode.vrefresh) * 1000u;
  if (mode.clock > 0 && mode.htotal > 0 && mode.vtotal > 0) {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(mode.clock) * 1'000'000ull) /
        (static_cast<std::uint64_t>(mode.htotal) * static_cast<std::uint64_t>(mode.vtotal)));
  }
  return 60'000u;
}

bool displayNameMatches(char const* displayName, KmsConnector const& connector) {
  if (!displayName || connector.name.empty()) return false;
  return std::string(displayName).find(connector.name) != std::string::npos;
}

bool displaySizeMatches(VkDisplayPropertiesKHR const& display, KmsConnector const& connector) {
  if (connector.widthMm <= 0 || connector.heightMm <= 0) return false;
  int const widthDelta = std::abs(static_cast<int>(display.physicalDimensions.width) - connector.widthMm);
  int const heightDelta = std::abs(static_cast<int>(display.physicalDimensions.height) - connector.heightMm);
  return widthDelta <= 5 && heightDelta <= 5;
}

bool modeResolutionMatches(VkDisplayModePropertiesKHR const& mode, KmsConnector const& connector) {
  return mode.parameters.visibleRegion.width == connector.mode.hdisplay &&
         mode.parameters.visibleRegion.height == connector.mode.vdisplay;
}

std::uint32_t chooseAlphaMode(VkDisplayPlaneCapabilitiesKHR const& caps) {
  if ((caps.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR) != 0) {
    return VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
  }
  if ((caps.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR) != 0) {
    return VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
  }
  if ((caps.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR) != 0) {
    return VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR;
  }
  return VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR;
}

VkSurfaceTransformFlagBitsKHR chooseTransform(VkDisplayPropertiesKHR const& display) {
  if ((display.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0) {
    return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }
  if ((display.supportedTransforms & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) != 0) {
    return VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
  }
  if ((display.supportedTransforms & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) != 0) {
    return VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR;
  }
  return VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;
}

bool planeSupportsDisplay(VkPhysicalDevice physical, std::uint32_t plane, VkDisplayKHR display) {
  std::uint32_t supportedCount = 0;
  if (vkGetDisplayPlaneSupportedDisplaysKHR(physical, plane, &supportedCount, nullptr) != VK_SUCCESS ||
      supportedCount == 0) {
    return false;
  }
  std::vector<VkDisplayKHR> supported(supportedCount);
  if (vkGetDisplayPlaneSupportedDisplaysKHR(physical, plane, &supportedCount, supported.data()) != VK_SUCCESS) {
    return false;
  }
  return std::find(supported.begin(), supported.end(), display) != supported.end();
}

bool instanceExtensionAvailable(char const* name) {
  std::uint32_t count = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) return false;
  std::vector<VkExtensionProperties> props(count);
  if (vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data()) != VK_SUCCESS) return false;
  return std::any_of(props.begin(), props.end(), [&](VkExtensionProperties const& prop) {
    return std::strcmp(prop.extensionName, name) == 0;
  });
}

char const* vkResultName(VkResult result) {
  switch (result) {
  case VK_SUCCESS: return "VK_SUCCESS";
  case VK_NOT_READY: return "VK_NOT_READY";
  case VK_TIMEOUT: return "VK_TIMEOUT";
  case VK_EVENT_SET: return "VK_EVENT_SET";
  case VK_EVENT_RESET: return "VK_EVENT_RESET";
  case VK_INCOMPLETE: return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  default: return "VkResult";
  }
}

VkDisplayPropertiesKHR drmMappedDisplayProperties(VkDisplayKHR display, KmsConnector const& connector) {
  static char const name[] = "DRM connector";
  VkDisplayPropertiesKHR props{};
  props.display = display;
  props.displayName = name;
  props.physicalDimensions = {static_cast<std::uint32_t>(std::max(0, connector.widthMm)),
                              static_cast<std::uint32_t>(std::max(0, connector.heightMm))};
  props.physicalResolution = {connector.mode.hdisplay, connector.mode.vdisplay};
  props.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  props.planeReorderPossible = VK_FALSE;
  props.persistentContent = VK_FALSE;
  return props;
}

VkSurfaceKHR tryCreateDisplaySurface(VkInstance instance, VkPhysicalDevice physical,
                                     VkDisplayPropertiesKHR const& display,
                                     VkDisplayModeKHR displayMode,
                                     VkExtent2D extent) {
  std::uint32_t planeCount = 0;
  if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical, &planeCount, nullptr) != VK_SUCCESS ||
      planeCount == 0) {
    return VK_NULL_HANDLE;
  }
  std::vector<VkDisplayPlanePropertiesKHR> planes(planeCount);
  vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical, &planeCount, planes.data());
  for (std::uint32_t plane = 0; plane < planeCount; ++plane) {
    if (!planeSupportsDisplay(physical, plane, display.display)) continue;
    VkDisplayPlaneCapabilitiesKHR caps{};
    VkResult capsResult = vkGetDisplayPlaneCapabilitiesKHR(physical, displayMode, plane, &caps);
    if (capsResult != VK_SUCCESS) continue;
    VkDisplaySurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR};
    info.displayMode = displayMode;
    info.planeIndex = plane;
    info.planeStackIndex = planes[plane].currentStackIndex;
    info.transform = chooseTransform(display);
    info.globalAlpha = 1.f;
    info.alphaMode = static_cast<VkDisplayPlaneAlphaFlagBitsKHR>(chooseAlphaMode(caps));
    info.imageExtent = extent;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult const surfaceResult = vkCreateDisplayPlaneSurfaceKHR(instance, &info, nullptr, &surface);
    if (surfaceResult == VK_SUCCESS) {
      return surface;
    }
  }
  return VK_NULL_HANDLE;
}

std::uint32_t chooseCrtc(int fd, drmModeRes* resources, drmModeConnector const& connector) {
  if (connector.encoder_id != 0) {
    if (drmModeEncoder* encoder = drmModeGetEncoder(fd, connector.encoder_id)) {
      std::uint32_t crtc = encoder->crtc_id;
      drmModeFreeEncoder(encoder);
      if (crtc != 0) return crtc;
    }
  }
  for (int i = 0; i < connector.count_encoders; ++i) {
    drmModeEncoder* encoder = drmModeGetEncoder(fd, connector.encoders[i]);
    if (!encoder) continue;
    for (int c = 0; c < resources->count_crtcs; ++c) {
      if ((encoder->possible_crtcs & (1 << c)) != 0) {
        std::uint32_t crtc = resources->crtcs[c];
        drmModeFreeEncoder(encoder);
        return crtc;
      }
    }
    drmModeFreeEncoder(encoder);
  }
  return 0;
}

} // namespace

KmsApplication::KmsApplication() {
  if (gKmsApplication) throw std::runtime_error("Only one KMS application can exist");
  gKmsApplication = this;
}

KmsApplication::~KmsApplication() {
  uninstallSignalHandlers();
  if (input_) libinput_unref(input_);
  if (udev_) udev_unref(udev_);
  if (drmFd_ >= 0) {
    drmDropMaster(drmFd_);
    close(drmFd_);
  }
  if (wakePipe_[0] >= 0) close(wakePipe_[0]);
  if (wakePipe_[1] >= 0) close(wakePipe_[1]);
  if (gKmsApplication == this) gKmsApplication = nullptr;
}

void KmsApplication::initialize() {
  if (pipe(wakePipe_) != 0) throw std::system_error(errno, std::generic_category(), "pipe");
  fcntl(wakePipe_[0], F_SETFL, fcntl(wakePipe_[0], F_GETFL, 0) | O_NONBLOCK);
  fcntl(wakePipe_[1], F_SETFL, fcntl(wakePipe_[1], F_GETFL, 0) | O_NONBLOCK);
  installSignalHandlers();
  if (!openFirstDisplayCard()) {
    throw std::runtime_error("No connected DRM/KMS display found");
  }
  if (drmSetMaster(drmFd_) != 0) {
    throw std::runtime_error("DRM master unavailable; another graphical session may be running.");
  }
  enumerateConnectors();
  initializeInput();
}

bool KmsApplication::openFirstDisplayCard() {
  if (!std::filesystem::exists("/dev/dri")) return false;
  for (auto const& entry : std::filesystem::directory_iterator("/dev/dri")) {
    std::string name = entry.path().filename().string();
    if (!name.starts_with("card")) continue;
    int fd = open(entry.path().c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) continue;
    drmModeRes* resources = drmModeGetResources(fd);
    bool hasConnected = false;
    if (resources) {
      for (int i = 0; i < resources->count_connectors && !hasConnected; ++i) {
        drmModeConnector* connector = drmModeGetConnector(fd, resources->connectors[i]);
        hasConnected = connector && connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0;
        if (connector) drmModeFreeConnector(connector);
      }
      drmModeFreeResources(resources);
    }
    if (hasConnected) {
      drmFd_ = fd;
      return true;
    }
    close(fd);
  }
  return false;
}

void KmsApplication::enumerateConnectors() {
  drmModeRes* resources = drmModeGetResources(drmFd_);
  if (!resources) throw std::runtime_error("drmModeGetResources failed");
  for (int i = 0; i < resources->count_connectors; ++i) {
    drmModeConnector* connector = drmModeGetConnector(drmFd_, resources->connectors[i]);
    if (!connector) continue;
    if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
      KmsConnector out{};
      out.connectorId = connector->connector_id;
      out.encoderId = connector->encoder_id;
      out.crtcId = chooseCrtc(drmFd_, resources, *connector);
      out.mode = chooseMode(*connector);
      out.widthMm = connector->mmWidth;
      out.heightMm = connector->mmHeight;
      out.name = connectorName(*connector);
      if (out.crtcId != 0) connectors_.push_back(out);
    }
    drmModeFreeConnector(connector);
  }
  drmModeFreeResources(resources);
  if (connectors_.empty()) throw std::runtime_error("No usable DRM/KMS connector found");
}

void KmsApplication::initializeInput() {
  udev_ = udev_new();
  if (!udev_) throw std::runtime_error("udev_new failed");
  input_ = libinput_udev_create_context(&kLibinputInterface, nullptr, udev_);
  if (!input_) throw std::runtime_error("libinput_udev_create_context failed");
  if (libinput_udev_assign_seat(input_, "seat0") != 0) {
    throw std::runtime_error("libinput_udev_assign_seat(seat0) failed");
  }
}

void KmsApplication::setApplicationName(std::string name) {
  appName_ = sanitizeAppName(std::move(name));
}

std::string KmsApplication::applicationName() const {
  return appName_.empty() ? "flux" : appName_;
}

void KmsApplication::setMenuBar(MenuBar const& menu, MenuActionDispatcher dispatcher) {
  claimedShortcuts_.clear();
  collectShortcuts(menu);
  dispatcher_ = std::move(dispatcher);
}

void KmsApplication::setTerminateHandler(std::function<void()> handler) {
  terminateHandler_ = std::move(handler);
}

void KmsApplication::requestTerminate() {
  if (!terminateRequested_.exchange(true) && terminateHandler_) {
    terminateHandler_();
  }
  wakeEventLoop();
}

std::unordered_set<ShortcutKey, ShortcutKeyHash> KmsApplication::menuClaimedShortcuts() const {
  return claimedShortcuts_;
}

void KmsApplication::revalidateMenuItems(std::function<bool(std::string const&)>) {}

std::string KmsApplication::userDataDir() const {
  return appDir(envOr("XDG_DATA_HOME", envOr("HOME", ".") + "/.local/share"), applicationName());
}

std::string KmsApplication::cacheDir() const {
  return appDir(envOr("XDG_CACHE_HOME", envOr("HOME", ".") + "/.cache"), applicationName());
}

std::span<char const* const> KmsApplication::requiredVulkanInstanceExtensions() const {
  static std::vector<char const*> exts = [] {
    std::vector<char const*> result{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_DISPLAY_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    if (instanceExtensionAvailable(VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME)) {
      result.push_back(VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME);
    }
    if (instanceExtensionAvailable(VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME)) {
      result.push_back(VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME);
    }
    return result;
  }();
  return exts;
}

VkSurfaceKHR KmsApplication::createVulkanSurface(VkInstance instance, void* nativeHandle) {
  auto* connector = static_cast<KmsConnector*>(nativeHandle);
  if (!connector) throw std::runtime_error("Invalid KMS Vulkan surface handle");

  std::uint32_t deviceCount = 0;
  vkCheck(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices");
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkCheck(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices");

  struct DisplayCandidate {
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDisplayPropertiesKHR display{};
    int score = 0;
  };
  std::vector<DisplayCandidate> candidates;
  auto getDrmDisplay = reinterpret_cast<PFN_vkGetDrmDisplayEXT>(
      vkGetInstanceProcAddr(instance, "vkGetDrmDisplayEXT"));
  auto acquireDrmDisplay = reinterpret_cast<PFN_vkAcquireDrmDisplayEXT>(
      vkGetInstanceProcAddr(instance, "vkAcquireDrmDisplayEXT"));
  bool const hasDrmDisplayExtension = getDrmDisplay != nullptr;
  bool drmDisplayMapped = false;
  std::vector<std::string> diagnostics;

  for (VkPhysicalDevice physical : devices) {
    VkPhysicalDeviceProperties physicalProps{};
    vkGetPhysicalDeviceProperties(physical, &physicalProps);
    if (getDrmDisplay) {
      VkDisplayKHR drmDisplay = VK_NULL_HANDLE;
      VkResult const getResult = getDrmDisplay(physical, drmFd_, connector->connectorId, &drmDisplay);
      diagnostics.push_back(std::string("device ") + physicalProps.deviceName + ": vkGetDrmDisplayEXT=" +
                            vkResultName(getResult));
      if (getResult == VK_SUCCESS && drmDisplay != VK_NULL_HANDLE) {
        drmDisplayMapped = true;
        if (acquireDrmDisplay) {
          VkResult const acquireResult = acquireDrmDisplay(physical, drmFd_, drmDisplay);
          if (acquireResult != VK_SUCCESS && acquireResult != VK_ERROR_INITIALIZATION_FAILED) {
            std::fprintf(stderr,
                         "Flux KMS: vkAcquireDrmDisplayEXT failed for connector %s with %s (%d); trying surface creation anyway.\n",
                         connector->name.c_str(), vkResultName(acquireResult), static_cast<int>(acquireResult));
          }
        }
        candidates.push_back(DisplayCandidate{physical, drmMappedDisplayProperties(drmDisplay, *connector), 20'000});
      }
    }

    std::uint32_t displayCount = 0;
    if (vkGetPhysicalDeviceDisplayPropertiesKHR(physical, &displayCount, nullptr) != VK_SUCCESS || displayCount == 0) {
      diagnostics.push_back(std::string("device ") + physicalProps.deviceName + ": no VK_KHR_display displays");
      continue;
    }
    std::vector<VkDisplayPropertiesKHR> displays(displayCount);
    vkGetPhysicalDeviceDisplayPropertiesKHR(physical, &displayCount, displays.data());
    diagnostics.push_back(std::string("device ") + physicalProps.deviceName + ": " +
                          std::to_string(displayCount) + " VK_KHR_display displays");
    for (auto const& display : displays) {
      int score = 0;
      if (displayNameMatches(display.displayName, *connector)) score += 1000;
      if (displaySizeMatches(display, *connector)) score += 300;
      if (display.physicalResolution.width == connector->mode.hdisplay &&
          display.physicalResolution.height == connector->mode.vdisplay) {
        score += 100;
      }
      candidates.push_back(DisplayCandidate{physical, display, score});
    }
  }
  std::sort(candidates.begin(), candidates.end(),
            [](DisplayCandidate const& a, DisplayCandidate const& b) { return a.score > b.score; });

  for (auto const& candidate : candidates) {
    std::uint32_t modeCount = 0;
    if (vkGetDisplayModePropertiesKHR(candidate.physical, candidate.display.display, &modeCount, nullptr) !=
            VK_SUCCESS ||
        modeCount == 0) {
      continue;
    }
    std::vector<VkDisplayModePropertiesKHR> modes(modeCount);
    vkGetDisplayModePropertiesKHR(candidate.physical, candidate.display.display, &modeCount, modes.data());
    std::sort(modes.begin(), modes.end(), [&](auto const& a, auto const& b) {
      int const aExact = modeResolutionMatches(a, *connector) ? 1 : 0;
      int const bExact = modeResolutionMatches(b, *connector) ? 1 : 0;
      return aExact > bExact;
    });
    for (auto const& mode : modes) {
      if (!modeResolutionMatches(mode, *connector) && candidate.score == 0) {
        continue;
      }
      if (VkSurfaceKHR surface = tryCreateDisplaySurface(instance, candidate.physical, candidate.display,
                                                         mode.displayMode, mode.parameters.visibleRegion)) {
        return surface;
      }
    }
  }

  for (auto const& candidate : candidates) {
    VkDisplayModeCreateInfoKHR modeInfo{VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR};
    modeInfo.parameters.visibleRegion = {connector->mode.hdisplay, connector->mode.vdisplay};
    modeInfo.parameters.refreshRate = refreshRateMilliHz(connector->mode);
    VkDisplayModeKHR createdMode = VK_NULL_HANDLE;
    if (vkCreateDisplayModeKHR(candidate.physical, candidate.display.display, &modeInfo, nullptr, &createdMode) !=
        VK_SUCCESS) {
      continue;
    }
    if (VkSurfaceKHR surface = tryCreateDisplaySurface(instance, candidate.physical, candidate.display, createdMode,
                                                       modeInfo.parameters.visibleRegion)) {
      return surface;
    }
  }

  std::fprintf(stderr,
               "Flux KMS: no Vulkan display surface for connector %s id=%u (%ux%u @ %u mHz); "
               "VK_EXT_acquire_drm_display=%s mapped=%s candidates=%zu.\n",
               connector->name.c_str(), connector->connectorId, connector->mode.hdisplay,
               connector->mode.vdisplay, refreshRateMilliHz(connector->mode),
               hasDrmDisplayExtension ? "yes" : "no", drmDisplayMapped ? "yes" : "no", candidates.size());
  for (std::string const& line : diagnostics) {
    std::fprintf(stderr, "Flux KMS: %s\n", line.c_str());
  }
  throw std::runtime_error("No Vulkan display surface matched the selected KMS connector");
}

int KmsApplication::inputFd() const noexcept {
  return input_ ? libinput_get_fd(input_) : -1;
}

void KmsApplication::wakeEventLoop() {
  if (wakePipe_[1] < 0) return;
  char const c = 1;
  (void)write(wakePipe_[1], &c, 1);
}

void KmsApplication::drainWakePipe() {
  char buffer[64];
  while (read(wakePipe_[0], buffer, sizeof(buffer)) > 0) {}
}

void KmsApplication::installSignalHandlers() {
  if (signalHandlersInstalled_) return;
  gSignalWakeFd = wakePipe_[1];
  gTerminateSignalPending = 0;

  struct sigaction action {};
  sigemptyset(&action.sa_mask);
  action.sa_handler = terminateSignalHandler;

  if (sigaction(SIGINT, &action, &gTerminationSignals.previousSigInt) != 0) {
    gSignalWakeFd = -1;
    throw std::system_error(errno, std::generic_category(), "sigaction(SIGINT)");
  }
  gTerminationSignals.sigIntInstalled = true;

  if (sigaction(SIGTERM, &action, &gTerminationSignals.previousSigTerm) != 0) {
    int const savedErrno = errno;
    restoreTerminationSignalHandlers();
    throw std::system_error(savedErrno, std::generic_category(), "sigaction(SIGTERM)");
  }
  gTerminationSignals.sigTermInstalled = true;
  signalHandlersInstalled_ = true;
}

void KmsApplication::uninstallSignalHandlers() {
  if (!signalHandlersInstalled_) return;
  restoreTerminationSignalHandlers();
  signalHandlersInstalled_ = false;
}

void KmsApplication::handlePendingTerminateSignal() {
  if (!gTerminateSignalPending) return;
  gTerminateSignalPending = 0;
  requestTerminate();
}

bool KmsApplication::pollInputAndWake(int timeoutMs, std::span<int const> extraFds) {
  std::vector<pollfd> fds;
  if (inputFd() >= 0) fds.push_back({inputFd(), POLLIN, 0});
  if (wakePipe_[0] >= 0) fds.push_back({wakePipe_[0], POLLIN, 0});
  for (int fd : extraFds) {
    if (fd >= 0) fds.push_back({fd, POLLIN, 0});
  }
  for (KmsWindow const* window : windows_) {
    int const timerFd = window ? window->frameTimerFd() : -1;
    if (timerFd >= 0) {
      bool const alreadyPolled = std::any_of(fds.begin(), fds.end(), [&](pollfd const& pollFd) {
        return pollFd.fd == timerFd;
      });
      if (!alreadyPolled) fds.push_back({timerFd, POLLIN, 0});
    }
  }
  int rc = poll(fds.data(), fds.size(), timeoutMs < 0 ? -1 : timeoutMs);
  if (rc < 0) {
    if (errno == EINTR) {
      handlePendingTerminateSignal();
      return true;
    }
    return false;
  }
  if (rc == 0) {
    handlePendingTerminateSignal();
    return false;
  }
  for (auto const& fd : fds) {
    if (fd.fd == wakePipe_[0] && (fd.revents & POLLIN)) drainWakePipe();
  }
  handlePendingTerminateSignal();
  dispatchPendingInput();
  return true;
}

void KmsApplication::registerWindow(KmsWindow* window) {
  windows_.push_back(window);
  if (!pointerFocus_) pointerFocus_ = window;
}

void KmsApplication::unregisterWindow(KmsWindow* window) {
  windows_.erase(std::remove(windows_.begin(), windows_.end(), window), windows_.end());
  if (pointerFocus_ == window) pointerFocus_ = windows_.empty() ? nullptr : windows_.front();
}

KmsWindow* KmsApplication::focusedWindow() const {
  return pointerFocus_ ? pointerFocus_ : (windows_.empty() ? nullptr : windows_.front());
}

void KmsApplication::collectShortcuts(MenuItem const& item) {
  if (!item.actionName.empty() && (item.shortcut.key != 0 || item.shortcut.modifiers != Modifiers::None)) {
    claimedShortcuts_.insert(ShortcutKey{.key = item.shortcut.key, .modifiers = item.shortcut.modifiers});
  }
  for (MenuItem const& child : item.children) collectShortcuts(child);
}

void KmsApplication::collectShortcuts(MenuBar const& menu) {
  for (MenuItem const& item : menu.menus) collectShortcuts(item);
}

KmsApplication& kmsApplication() {
  if (!gKmsApplication) throw std::runtime_error("KMS application is not initialized");
  return *gKmsApplication;
}

namespace detail {

std::unique_ptr<PlatformApplication> createPlatformApplication() {
  return std::make_unique<KmsApplication>();
}

} // namespace detail
} // namespace flux
