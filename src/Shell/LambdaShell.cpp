#define VK_USE_PLATFORM_WAYLAND_KHR

#include <Flux/Core/Color.hpp>
#include <Flux/Core/Geometry.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Graphics/VulkanContext.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/UI/Detail/RootHolder.hpp>
#include <Flux/UI/IconName.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "Graphics/Linux/FreeTypeTextSystem.hpp"
#include "Graphics/Vulkan/VulkanCanvas.hpp"
#include "Compositor/Protocols/ext-background-effect-v1-client-protocol.h"
#include "Compositor/Protocols/viewporter-client-protocol.h"
#include "Compositor/Protocols/wlr-layer-shell-unstable-v1-client-protocol.h"

#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kTopBarHeight = 36;
constexpr int kDockBottom = 12;
constexpr int kDockCell = 48;
constexpr int kDockPaddingX = 10;
constexpr int kDockPaddingY = 8;
constexpr int kDockGap = 6;
constexpr int kDockSeparatorWidth = 1;
std::atomic<bool> gRunning{true};

enum class SurfaceKind { TopBar, Dock, Launcher };

struct DockItem {
  std::string id;
  std::string kind;
  std::string label;
  std::string appId;
  bool running = false;
  bool focused = false;
  bool disabled = false;
};

struct ShellClient;

struct ShellSurface {
  SurfaceKind kind = SurfaceKind::TopBar;
  wl_surface* surface = nullptr;
  zwlr_layer_surface_v1* layer = nullptr;
  ext_background_effect_surface_v1* backgroundEffect = nullptr;
  wp_viewport* viewport = nullptr;
  VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
  std::unique_ptr<flux::Canvas> canvas;
  int width = 1;
  int height = 1;
  float dpiScale = 1.f;
  bool configured = false;
};

struct ShellClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_seat* seat = nullptr;
  wl_pointer* pointer = nullptr;
  wl_keyboard* keyboard = nullptr;
  zwlr_layer_shell_v1* layerShell = nullptr;
  ext_background_effect_manager_v1* backgroundEffectManager = nullptr;
  wp_viewporter* viewporter = nullptr;
  xkb_context* xkbContext = nullptr;
  xkb_keymap* xkbKeymap = nullptr;
  xkb_state* xkbState = nullptr;
  flux::FreeTypeTextSystem textSystem{[] { return std::string{"lambda-shell"}; }};
  ShellSurface topbar{.kind = SurfaceKind::TopBar};
  ShellSurface dock{.kind = SurfaceKind::Dock};
  ShellSurface launcher{.kind = SurfaceKind::Launcher};
  bool launcherOpen = false;
  bool launcherCreated = false;
  bool launcherModalClaimed = false;
  bool redrawTopbar = true;
  bool redrawDock = true;
  bool redrawLauncher = true;
  std::string query;
  int highlighted = 0;
  std::string activeTitle;
  std::string network = "unknown";
  std::string wifi = "unknown";
  std::string bluetooth = "unknown";
  std::string volume = "unknown";
  std::string battery = "unknown";
  float outputScale = 1.f;
  SurfaceKind pointerSurface = SurfaceKind::TopBar;
  double pointerX = -1.0;
  double pointerY = -1.0;
  int ipcFd = -1;
  std::string ipcBuffer;
  std::vector<DockItem> dockItems;
};

bool appIdMatches(std::string_view requested, std::string_view actual) {
  if (requested == actual) return true;
  if (requested == "terminal" && actual == "foot") return true;
  if (requested == "browser" && actual == "firefox") return true;
  if (requested == "files" && (actual == "org.gnome.Nautilus" || actual == "nautilus" || actual == "thunar")) return true;
  return false;
}

void vkCheck(VkResult result, char const* what) {
  if (result != VK_SUCCESS) throw std::runtime_error(std::string(what) + " failed");
}

std::string runtimePath(char const* name) {
  if (char const* runtimeDir = std::getenv("XDG_RUNTIME_DIR"); runtimeDir && *runtimeDir) {
    return std::string(runtimeDir) + "/" + name;
  }
  return std::string("/tmp/") + name;
}

std::string displayName() {
  if (char const* display = std::getenv("WAYLAND_DISPLAY"); display && *display) return display;
  FILE* file = std::fopen(runtimePath("lambda-window-manager-display").c_str(), "r");
  if (!file) return {};
  char buffer[128]{};
  std::fgets(buffer, sizeof(buffer), file);
  std::fclose(file);
  std::string name(buffer);
  while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
  return name;
}

void sendIpc(ShellClient& client, std::string const& line) {
  if (client.ipcFd < 0) return;
  std::string payload = line;
  payload.push_back('\n');
  (void)write(client.ipcFd, payload.data(), payload.size());
}

std::string escapeJson(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '\\' || c == '"') out.push_back('\\');
    if (static_cast<unsigned char>(c) >= 0x20u) out.push_back(c);
  }
  return out;
}

std::string utf8(char32_t codepoint) {
  std::string out;
  if (codepoint <= 0x7Fu) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FFu) {
    out.push_back(static_cast<char>(0xC0u | (codepoint >> 6u)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  } else if (codepoint <= 0xFFFFu) {
    out.push_back(static_cast<char>(0xE0u | (codepoint >> 12u)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  } else {
    out.push_back(static_cast<char>(0xF0u | (codepoint >> 18u)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  }
  return out;
}

std::string icon(flux::IconName name) {
  return utf8(static_cast<char32_t>(name));
}

std::string jsonStringField(std::string_view line, std::string_view name, std::size_t start = 0) {
  std::string const key = "\"" + std::string(name) + "\"";
  std::size_t pos = line.find(key, start);
  if (pos == std::string_view::npos) return {};
  pos = line.find(':', pos + key.size());
  if (pos == std::string_view::npos) return {};
  pos = line.find('"', pos + 1u);
  if (pos == std::string_view::npos) return {};
  std::string out;
  bool escaping = false;
  for (++pos; pos < line.size(); ++pos) {
    char const c = line[pos];
    if (escaping) {
      out.push_back(c);
      escaping = false;
    } else if (c == '\\') {
      escaping = true;
    } else if (c == '"') {
      break;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

float jsonFloatField(std::string_view line, std::string_view name, float fallback) {
  std::string const key = "\"" + std::string(name) + "\"";
  std::size_t pos = line.find(key);
  if (pos == std::string_view::npos) return fallback;
  pos = line.find(':', pos + key.size());
  if (pos == std::string_view::npos) return fallback;
  ++pos;
  while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
  std::size_t end = pos;
  while (end < line.size() &&
         (std::isdigit(static_cast<unsigned char>(line[end])) || line[end] == '.' ||
          line[end] == '-' || line[end] == '+')) {
    ++end;
  }
  if (end == pos) return fallback;
  std::string value{line.substr(pos, end - pos)};
  char* parseEnd = nullptr;
  float parsed = std::strtof(value.c_str(), &parseEnd);
  if (parseEnd == value.c_str()) return fallback;
  return std::clamp(parsed, 0.5f, 4.f);
}

void setupVulkanRuntime() {
  static bool initialized = false;
  if (initialized) return;
  static constexpr char const* exts[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
  };
  std::filesystem::path cacheDir = std::filesystem::path(std::getenv("XDG_CACHE_HOME") ? std::getenv("XDG_CACHE_HOME") : "/tmp") /
                                  "lambda-shell";
  flux::configureVulkanCanvasRuntime(exts, cacheDir);
  initialized = true;
}

void updateViewportDestination(ShellClient& client, ShellSurface& surface) {
  if (!client.viewporter || !surface.surface || surface.width <= 0 || surface.height <= 0) return;
  if (!surface.viewport) surface.viewport = wp_viewporter_get_viewport(client.viewporter, surface.surface);
  int const logicalWidth = std::max(1, surface.width);
  int const logicalHeight = std::max(1, surface.height);
  int const sourceWidth = std::max(1, static_cast<int>(std::lround(static_cast<float>(logicalWidth) * client.outputScale)));
  int const sourceHeight = std::max(1, static_cast<int>(std::lround(static_cast<float>(logicalHeight) * client.outputScale)));
  wp_viewport_set_source(surface.viewport,
                         wl_fixed_from_int(0),
                         wl_fixed_from_int(0),
                         wl_fixed_from_int(sourceWidth),
                         wl_fixed_from_int(sourceHeight));
  wp_viewport_set_destination(surface.viewport, logicalWidth, logicalHeight);
}

void ensureCanvas(ShellClient& client, ShellSurface& surface) {
  if (surface.canvas) {
    if (std::abs(surface.dpiScale - client.outputScale) > 0.001f) {
      surface.dpiScale = client.outputScale;
      surface.canvas->updateDpiScale(surface.dpiScale, surface.dpiScale);
    }
    surface.canvas->resize(surface.width, surface.height);
    updateViewportDestination(client, surface);
    return;
  }
  setupVulkanRuntime();
  VkWaylandSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
  info.display = client.display;
  info.surface = surface.surface;
  vkCheck(vkCreateWaylandSurfaceKHR(flux::ensureSharedVulkanInstance(), &info, nullptr, &surface.vkSurface),
          "vkCreateWaylandSurfaceKHR");
  surface.canvas = flux::createVulkanCanvas(surface.vkSurface,
                                            static_cast<unsigned int>(static_cast<int>(surface.kind) + 100u),
                                            client.textSystem);
  surface.dpiScale = client.outputScale;
  surface.canvas->updateDpiScale(surface.dpiScale, surface.dpiScale);
  surface.canvas->resize(surface.width, surface.height);
  updateViewportDestination(client, surface);
}

void updateBackgroundEffect(ShellClient& client, ShellSurface& surface) {
  if (!client.backgroundEffectManager || !surface.surface || surface.width <= 0 || surface.height <= 0) return;
  if (!surface.backgroundEffect) {
    surface.backgroundEffect = ext_background_effect_manager_v1_get_background_effect(client.backgroundEffectManager,
                                                                                     surface.surface);
  }
  wl_region* region = wl_compositor_create_region(client.compositor);
  wl_region_add(region, 0, 0, surface.width, surface.height);
  ext_background_effect_surface_v1_set_blur_region(surface.backgroundEffect, region);
  wl_region_destroy(region);
}

flux::Color rgba(float r, float g, float b, float a) {
  return flux::Color{r, g, b, a};
}

flux::FillStyle gradient(flux::Color from, flux::Color to) {
  return flux::FillStyle::linearGradient(from, to, {0.f, 0.f}, {1.f, 1.f});
}

void requestRender(ShellClient& client, SurfaceKind surface) {
  if (surface == SurfaceKind::TopBar) client.redrawTopbar = true;
  else if (surface == SurfaceKind::Dock) client.redrawDock = true;
  else client.redrawLauncher = true;
}

void requestRenderAll(ShellClient& client) {
  client.redrawTopbar = true;
  client.redrawDock = true;
  client.redrawLauncher = true;
}

void drawText(ShellClient& client,
              flux::Canvas& canvas,
              std::string_view value,
              flux::Font font,
              flux::Color color,
              flux::Rect rect,
              flux::TextLayoutOptions options = {}) {
  flux::TextSystem& textSystem = client.textSystem;
  if (auto layout = textSystem.layout(value, font, color, rect, options)) {
    canvas.save();
    canvas.clipRect(rect);
    canvas.drawTextLayout(*layout, {0.f, 0.f});
    canvas.restore();
  }
}

flux::TextLayoutOptions centeredText() {
  return flux::TextLayoutOptions{.horizontalAlignment = flux::HorizontalAlignment::Center,
                                 .verticalAlignment = flux::VerticalAlignment::Center,
                                 .wrapping = flux::TextWrapping::NoWrap};
}

struct TopBarContent {
  std::string title;
  std::string timeText;

  flux::Element body() const {
    flux::Color const text = rgba(1.f, 1.f, 1.f, 1.f);
    std::vector<flux::Element> statusItems;
    statusItems.reserve(4);
    for (flux::IconName name : {flux::IconName::NetworkWifi,
                                flux::IconName::Bluetooth,
                                flux::IconName::VolumeUp,
                                flux::IconName::BatteryFull}) {
      statusItems.push_back(flux::Text{
          .text = icon(name),
          .font = flux::Font{.family = "Material Symbols Rounded", .size = 22.f, .weight = 900.f},
          .color = text,
          .horizontalAlignment = flux::HorizontalAlignment::Center,
          .verticalAlignment = flux::VerticalAlignment::Center,
      }.size(23.f, 23.f));
    }

    return flux::HStack{
        .spacing = 10.f,
        .alignment = flux::Alignment::Center,
        .children = flux::children(
            flux::Text{
                .text = "λ",
                .font = flux::Font{.size = 20.f, .weight = 980.f},
                .color = text,
                .horizontalAlignment = flux::HorizontalAlignment::Center,
                .verticalAlignment = flux::VerticalAlignment::Center,
            }.size(22.f, static_cast<float>(kTopBarHeight)),
            flux::Text{
                .text = title,
                .font = flux::Font{.size = 13.5f, .weight = 920.f},
                .color = text,
                .verticalAlignment = flux::VerticalAlignment::Center,
            }.flex(1.f, 1.f, 0.f),
            flux::HStack{
                .spacing = 6.f,
                .alignment = flux::Alignment::Center,
                .children = std::move(statusItems),
            },
            flux::Text{
                .text = timeText,
                .font = flux::Font{.size = 13.5f, .weight = 920.f},
                .color = text,
                .horizontalAlignment = flux::HorizontalAlignment::Trailing,
                .verticalAlignment = flux::VerticalAlignment::Center,
            }.width(176.f)),
    }.padding(0.f, 12.f, 0.f, 14.f)
     .height(static_cast<float>(kTopBarHeight));
  }
};

void drawTopbar(ShellClient& client) {
  auto& s = client.topbar;
  if (!s.configured || !client.redrawTopbar) return;
  client.redrawTopbar = false;
  ensureCanvas(client, s);
  updateBackgroundEffect(client, s);
  auto& canvas = *s.canvas;
  canvas.beginFrame();
  canvas.clear(flux::Colors::transparent);
  char timeText[64]{};
  std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  std::strftime(timeText, sizeof(timeText), "%a %d %b, %H:%M", &local);
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<TopBarContent>>(std::in_place,
                                                             TopBarContent{client.activeTitle, timeText}),
      client.textSystem,
      flux::EnvironmentBinding{},
      flux::Size{static_cast<float>(s.width), static_cast<float>(s.height)},
      [&client] { requestRender(client, SurfaceKind::TopBar); },
  };
  root.mount(sceneGraph);
  flux::scenegraph::SceneRenderer renderer{canvas};
  renderer.render(sceneGraph);
  canvas.present();
}

int dockWidth(ShellClient const& client) {
  int width = kDockPaddingX * 2;
  for (std::size_t i = 0; i < client.dockItems.size(); ++i) {
    width += client.dockItems[i].kind == "separator" ? kDockSeparatorWidth : kDockCell;
    if (i + 1 < client.dockItems.size()) width += kDockGap;
  }
  return width;
}

int dockItemWidth(DockItem const& item) {
  return item.kind == "separator" ? kDockSeparatorWidth : kDockCell;
}

struct IconPalette {
  flux::Color from;
  flux::Color to;
  flux::Color ink;
};

IconPalette iconPalette(DockItem const& item) {
  if (item.kind == "launcher") return {rgba(0.96f, 0.97f, 0.99f, 1.f), rgba(0.80f, 0.84f, 0.92f, 1.f), rgba(0.13f, 0.20f, 0.33f, 1.f)};
  if (item.appId == "files") return {rgba(0.43f, 0.65f, 1.f, 1.f), rgba(0.16f, 0.50f, 1.f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.appId == "browser") return {rgba(0.37f, 0.72f, 1.f, 1.f), rgba(0.16f, 0.53f, 1.f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.appId == "terminal") return {rgba(0.16f, 0.18f, 0.27f, 1.f), rgba(0.05f, 0.07f, 0.14f, 1.f), rgba(0.75f, 0.90f, 1.f, 1.f)};
  if (item.appId == "settings") return {rgba(0.58f, 0.63f, 0.72f, 1.f), rgba(0.31f, 0.35f, 0.45f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.appId == "calendar") return {rgba(1.f, 1.f, 1.f, 1.f), rgba(0.92f, 0.94f, 0.98f, 1.f), rgba(0.90f, 0.29f, 0.24f, 1.f)};
  if (item.appId == "mail") return {rgba(0.44f, 0.71f, 1.f, 1.f), rgba(0.16f, 0.53f, 1.f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.appId == "music") return {rgba(0.79f, 0.50f, 0.90f, 1.f), rgba(0.48f, 0.25f, 1.f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.kind == "trash") return {rgba(0.80f, 0.84f, 0.89f, 1.f), rgba(0.58f, 0.64f, 0.74f, 1.f), rgba(0.10f, 0.15f, 0.25f, 1.f)};
  return {rgba(0.96f, 0.97f, 0.99f, 1.f), rgba(0.82f, 0.86f, 0.93f, 1.f), rgba(0.10f, 0.15f, 0.25f, 1.f)};
}

flux::IconName dockIconName(DockItem const& item) {
  if (item.kind == "launcher") return flux::IconName::Dashboard;
  if (item.appId == "files") return flux::IconName::FolderOpen;
  if (item.appId == "browser") return flux::IconName::Globe;
  if (item.appId == "terminal") return flux::IconName::Terminal;
  if (item.appId == "settings") return flux::IconName::Tune;
  if (item.appId == "calendar") return flux::IconName::CalendarToday;
  if (item.appId == "mail") return flux::IconName::Mail;
  if (item.appId == "music") return flux::IconName::LibraryMusic;
  if (item.kind == "trash") return flux::IconName::DeleteForever;
  return flux::IconName::Apps;
}

void drawDockIconGlyph(ShellClient& client, flux::Canvas& canvas, DockItem const& item, flux::Rect iconRect, flux::Color ink) {
  flux::Font font{.family = "Material Symbols Rounded", .size = 31.f, .weight = 780.f};
  drawText(client, canvas, icon(dockIconName(item)), font, ink, iconRect, centeredText());
}

void drawDock(ShellClient& client) {
  auto& s = client.dock;
  s.width = dockWidth(client);
  s.height = kDockPaddingY * 2 + kDockCell + 8;
  if (s.layer) zwlr_layer_surface_v1_set_size(s.layer, static_cast<std::uint32_t>(s.width), static_cast<std::uint32_t>(s.height));
  if (!s.configured || !client.redrawDock) return;
  client.redrawDock = false;
  ensureCanvas(client, s);
  updateBackgroundEffect(client, s);
  auto& canvas = *s.canvas;
  canvas.beginFrame();
  canvas.clear(flux::Colors::transparent);
  int x = kDockPaddingX;
  flux::Font labelFont{.size = 13.f, .weight = 620.f};
  for (auto const& item : client.dockItems) {
    if (item.kind == "separator") {
      canvas.drawRect(flux::Rect::sharp(static_cast<float>(x), 15, kDockSeparatorWidth, 30),
                      flux::CornerRadius{},
                      flux::FillStyle::solid(rgba(1.f, 1.f, 1.f, 0.30f)),
                      flux::StrokeStyle::none());
    } else {
      bool const hover = client.pointerSurface == SurfaceKind::Dock &&
                         client.pointerX >= x && client.pointerX < x + kDockCell &&
                         client.pointerY >= kDockPaddingY && client.pointerY < kDockPaddingY + kDockCell;
      float const lift = hover ? -5.f : 0.f;
      flux::Rect icon = flux::Rect::sharp(static_cast<float>(x + 4), static_cast<float>(kDockPaddingY + 4) + lift, 40, 40);
      IconPalette const palette = iconPalette(item);
      canvas.drawRect(icon,
                      flux::CornerRadius{11.f},
                      gradient(palette.from, palette.to),
                      flux::StrokeStyle::solid(rgba(1.f, 1.f, 1.f, 0.68f), 0.9f),
                      flux::ShadowStyle::none());
      drawDockIconGlyph(client, canvas, item, icon, palette.ink);
      if (item.running) {
        canvas.drawCircle({static_cast<float>(x + kDockCell / 2), static_cast<float>(s.height - 7)},
                          2.f,
                          flux::FillStyle::solid(rgba(0.08f, 0.12f, 0.22f, item.focused ? 0.95f : 0.40f)),
                          flux::StrokeStyle::none());
      }
      if (hover) {
        float const labelW = static_cast<float>(item.label.size()) * 8.f + 24.f;
        float const tx = std::clamp(static_cast<float>(x + 24) - labelW * 0.5f, 0.f, static_cast<float>(s.width) - labelW);
        canvas.drawRect(flux::Rect::sharp(tx, 0, labelW, 27),
                        flux::CornerRadius{9.f},
                        flux::FillStyle::solid(rgba(0.10f, 0.13f, 0.18f, 0.84f)),
                        flux::StrokeStyle::none());
        flux::Path arrow;
        arrow.moveTo({static_cast<float>(x + 18), 27.f});
        arrow.lineTo({static_cast<float>(x + 30), 27.f});
        arrow.lineTo({static_cast<float>(x + 24), 34.f});
        arrow.close();
        canvas.drawPath(arrow, flux::FillStyle::solid(rgba(0.10f, 0.13f, 0.18f, 0.84f)), flux::StrokeStyle::none());
        drawText(client, canvas, item.label, labelFont, rgba(1, 1, 1, 1),
                 flux::Rect::sharp(tx + 10, 4, labelW - 20, 18), centeredText());
      }
    }
    x += dockItemWidth(item) + kDockGap;
  }
  canvas.present();
}

std::vector<DockItem> launcherResults(ShellClient const& client) {
  std::vector<DockItem> results;
  std::string q = client.query;
  std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  for (auto const& item : client.dockItems) {
    if (item.kind != "app") continue;
    std::string label = item.label;
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (q.empty() || label.find(q) != std::string::npos || item.appId.find(q) != std::string::npos) results.push_back(item);
  }
  return results;
}

struct LauncherLayout {
  float fieldX = 0.f;
  float fieldY = 80.f;
  float fieldW = 0.f;
  int tileW = 126;
  int tileH = 96;
  int gap = 12;
  int columns = 1;
  int gridW = 0;
  int startX = 0;
};

LauncherLayout launcherLayout(ShellSurface const& surface) {
  LauncherLayout layout;
  layout.fieldW = std::min(420.f, static_cast<float>(surface.width) - 48.f);
  layout.fieldX = (static_cast<float>(surface.width) - layout.fieldW) * 0.5f;
  layout.columns = std::max(1, std::min(4, (surface.width - 80) / (layout.tileW + layout.gap)));
  layout.gridW = layout.columns * layout.tileW + (layout.columns - 1) * layout.gap;
  layout.startX = (surface.width - layout.gridW) / 2;
  return layout;
}

std::optional<std::size_t> launcherResultAt(ShellClient const& client, std::vector<DockItem> const& results) {
  if (!client.launcherOpen || results.empty()) return std::nullopt;
  LauncherLayout const layout = launcherLayout(client.launcher);
  double const y = client.pointerY - (layout.fieldY + 76.0);
  if (y < 0.0) return std::nullopt;
  int const row = static_cast<int>(y) / (layout.tileH + layout.gap);
  int const col = (static_cast<int>(client.pointerX) - layout.startX) / (layout.tileW + layout.gap);
  if (col < 0 || col >= layout.columns) return std::nullopt;
  int const localX = static_cast<int>(client.pointerX) - layout.startX - col * (layout.tileW + layout.gap);
  int const localY = static_cast<int>(y) - row * (layout.tileH + layout.gap);
  if (localX < 0 || localX >= layout.tileW || localY < 0 || localY >= layout.tileH) return std::nullopt;
  std::size_t const index = static_cast<std::size_t>(row * layout.columns + col);
  if (index >= results.size()) return std::nullopt;
  return index;
}

bool launcherPointerInsideContent(ShellClient const& client, std::vector<DockItem> const& results) {
  LauncherLayout const layout = launcherLayout(client.launcher);
  bool const inField = client.pointerX >= layout.fieldX &&
                       client.pointerX < layout.fieldX + layout.fieldW &&
                       client.pointerY >= layout.fieldY &&
                       client.pointerY < layout.fieldY + 48.f;
  return inField || launcherResultAt(client, results).has_value();
}

void drawLauncher(ShellClient& client) {
  auto& s = client.launcher;
  if (!client.launcherOpen || !s.configured || !client.redrawLauncher) return;
  client.redrawLauncher = false;
  if (!client.launcherModalClaimed) {
    client.launcherModalClaimed = true;
    sendIpc(client, "{\"type\":\"lambda.windowManager.claimCommandLauncherModal\"}");
  }
  ensureCanvas(client, s);
  updateBackgroundEffect(client, s);
  auto& canvas = *s.canvas;
  canvas.beginFrame();
  canvas.clear(flux::Colors::transparent);
  canvas.drawRect(flux::Rect::sharp(0, 0, static_cast<float>(s.width), static_cast<float>(s.height)),
                  flux::CornerRadius{},
                  flux::FillStyle::solid(rgba(0.03f, 0.05f, 0.10f, 0.26f)),
                  flux::StrokeStyle::none());
  LauncherLayout const layout = launcherLayout(s);
  canvas.drawRect(flux::Rect::sharp(layout.fieldX, layout.fieldY, layout.fieldW, 48),
                  flux::CornerRadius{14.f},
                  flux::FillStyle::solid(rgba(1, 1, 1, 0.72f)),
                  flux::StrokeStyle::solid(rgba(1, 1, 1, 0.72f), 1.f),
                  flux::ShadowStyle::none());
  flux::Font inputFont{.size = 18.f, .weight = 540.f};
  drawText(client,
           canvas,
           client.query.empty() ? "Command" : client.query,
           inputFont,
           client.query.empty() ? rgba(0.32f, 0.34f, 0.40f, 1) : rgba(0.05f, 0.08f, 0.14f, 1),
           flux::Rect::sharp(layout.fieldX + 18, layout.fieldY + 12, layout.fieldW - 36, 24));
  auto results = launcherResults(client);
  client.highlighted = results.empty() ? 0 : std::clamp(client.highlighted, 0, static_cast<int>(results.size()) - 1);
  flux::Font tileFont{.size = 12.5f, .weight = 620.f};
  for (std::size_t i = 0; i < results.size(); ++i) {
    int const col = static_cast<int>(i) % layout.columns;
    int const row = static_cast<int>(i) / layout.columns;
    float const x = static_cast<float>(layout.startX + col * (layout.tileW + layout.gap));
    float const y = layout.fieldY + 76.f + static_cast<float>(row * (layout.tileH + layout.gap));
    bool const active = i == static_cast<std::size_t>(client.highlighted);
    canvas.drawRect(flux::Rect::sharp(x, y, layout.tileW, layout.tileH),
                    flux::CornerRadius{14.f},
                    flux::FillStyle::solid(active ? rgba(0.18f, 0.40f, 0.86f, 0.82f) : rgba(1, 1, 1, 0.54f)),
                    flux::StrokeStyle::none());
    flux::Rect icon = flux::Rect::sharp(x + 43, y + 14, 40, 40);
    IconPalette const palette = iconPalette(results[i]);
    canvas.drawRect(icon,
                    flux::CornerRadius{11.f},
                    active ? gradient(rgba(0.35f, 0.64f, 1.f, 1.f), rgba(0.13f, 0.40f, 0.93f, 1.f))
                           : gradient(palette.from, palette.to),
                    flux::StrokeStyle::solid(rgba(1.f, 1.f, 1.f, 0.68f), 0.9f));
    drawDockIconGlyph(client, canvas, results[i], icon, active ? rgba(1.f, 1.f, 1.f, 1.f) : palette.ink);
    drawText(client, canvas, results[i].label, tileFont,
             active ? rgba(1, 1, 1, 1) : rgba(0.08f, 0.12f, 0.22f, 1),
             flux::Rect::sharp(x + 14, y + 62, layout.tileW - 28, 18), centeredText());
  }
  canvas.present();
}

void renderAll(ShellClient& client) {
  drawTopbar(client);
  drawDock(client);
  drawLauncher(client);
  wl_display_flush(client.display);
}

void destroySurface(ShellSurface& surface) {
  SurfaceKind const kind = surface.kind;
  surface.canvas.reset();
  if (surface.viewport) wp_viewport_destroy(surface.viewport);
  if (surface.backgroundEffect) ext_background_effect_surface_v1_destroy(surface.backgroundEffect);
  if (surface.layer) zwlr_layer_surface_v1_destroy(surface.layer);
  if (surface.surface) wl_surface_destroy(surface.surface);
  surface = ShellSurface{.kind = kind};
}

void closeLauncher(ShellClient& client) {
  if (!client.launcherOpen) return;
  if (client.launcherModalClaimed) sendIpc(client, "{\"type\":\"lambda.windowManager.releaseCommandLauncherModal\"}");
  client.launcherOpen = false;
  client.launcherModalClaimed = false;
  client.query.clear();
  client.highlighted = 0;
  if (client.launcher.layer && client.launcher.surface) {
    zwlr_layer_surface_v1_set_keyboard_interactivity(client.launcher.layer, 0);
    zwlr_layer_surface_v1_set_size(client.launcher.layer, 1, 1);
    wl_surface_commit(client.launcher.surface);
  }
  requestRender(client, SurfaceKind::Dock);
  wl_display_flush(client.display);
}

void layerConfigure(void* data, zwlr_layer_surface_v1* layer, std::uint32_t serial, std::uint32_t width, std::uint32_t height) {
  auto* surface = static_cast<ShellSurface*>(data);
  zwlr_layer_surface_v1_ack_configure(layer, serial);
  surface->width = static_cast<int>(std::max(1u, width));
  surface->height = static_cast<int>(std::max(1u, height));
  surface->configured = true;
}

void layerClosed(void*, zwlr_layer_surface_v1*) {}
zwlr_layer_surface_v1_listener const kLayerListener{layerConfigure, layerClosed};

void createLayer(ShellClient& client, ShellSurface& surface, std::uint32_t layer, char const* ns) {
  surface.surface = wl_compositor_create_surface(client.compositor);
  wl_surface_set_buffer_scale(surface.surface, 1);
  if (client.viewporter) surface.viewport = wp_viewporter_get_viewport(client.viewporter, surface.surface);
  surface.layer = zwlr_layer_shell_v1_get_layer_surface(client.layerShell, surface.surface, nullptr, layer, ns);
  zwlr_layer_surface_v1_add_listener(surface.layer, &kLayerListener, &surface);
}

void openLauncher(ShellClient& client) {
  if (client.launcherOpen) return;
  client.launcherOpen = true;
  client.launcherModalClaimed = false;
  client.redrawLauncher = true;
  client.query.clear();
  client.highlighted = 0;
  if (!client.launcherCreated) {
    client.launcher = ShellSurface{.kind = SurfaceKind::Launcher};
    createLayer(client, client.launcher, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "lambda.command-launcher");
    client.launcherCreated = true;
  }
  zwlr_layer_surface_v1_set_anchor(client.launcher.layer,
                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
  zwlr_layer_surface_v1_set_size(client.launcher.layer, 0, 0);
  zwlr_layer_surface_v1_set_keyboard_interactivity(client.launcher.layer, 1);
  wl_surface_commit(client.launcher.surface);
  wl_display_flush(client.display);
}

void activateResult(ShellClient& client, DockItem const& item) {
  if (item.running) {
    sendIpc(client, "{\"type\":\"lambda.windowManager.focusApp\",\"appId\":\"" + escapeJson(item.appId) + "\"}");
  } else {
    sendIpc(client, "{\"type\":\"lambda.windowManager.launchApp\",\"appId\":\"" + escapeJson(item.appId) + "\"}");
  }
  closeLauncher(client);
}

void handleDockClick(ShellClient& client) {
  int x = kDockPaddingX;
  for (auto const& item : client.dockItems) {
    int const width = dockItemWidth(item);
    if (item.kind != "separator" && client.pointerX >= x && client.pointerX < x + width) {
      if (item.kind == "launcher") openLauncher(client);
      else if (item.kind == "app") activateResult(client, item);
      return;
    }
    x += width + kDockGap;
  }
}

void pointerEnter(void* data, wl_pointer*, std::uint32_t, wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy) {
  auto* client = static_cast<ShellClient*>(data);
  if (surface == client->dock.surface) client->pointerSurface = SurfaceKind::Dock;
  else if (surface == client->launcher.surface) client->pointerSurface = SurfaceKind::Launcher;
  else client->pointerSurface = SurfaceKind::TopBar;
  client->pointerX = wl_fixed_to_double(sx);
  client->pointerY = wl_fixed_to_double(sy);
  requestRender(*client, client->pointerSurface);
  if (client->pointerSurface == SurfaceKind::Dock) requestRender(*client, SurfaceKind::Launcher);
}

void pointerLeave(void* data, wl_pointer*, std::uint32_t, wl_surface*) {
  auto* client = static_cast<ShellClient*>(data);
  SurfaceKind const previous = client->pointerSurface;
  client->pointerX = -1;
  client->pointerY = -1;
  requestRender(*client, previous);
}

void pointerMotion(void* data, wl_pointer*, std::uint32_t, wl_fixed_t sx, wl_fixed_t sy) {
  auto* client = static_cast<ShellClient*>(data);
  client->pointerX = wl_fixed_to_double(sx);
  client->pointerY = wl_fixed_to_double(sy);
  if (client->pointerSurface == SurfaceKind::Launcher) {
    auto results = launcherResults(*client);
    if (auto index = launcherResultAt(*client, results)) {
      client->highlighted = static_cast<int>(*index);
      requestRender(*client, SurfaceKind::Launcher);
    }
  } else if (client->pointerSurface == SurfaceKind::Dock) {
    requestRender(*client, SurfaceKind::Dock);
  }
}

void pointerButton(void* data, wl_pointer*, std::uint32_t, std::uint32_t, std::uint32_t button, std::uint32_t state) {
  auto* client = static_cast<ShellClient*>(data);
  if (button != BTN_LEFT || state != WL_POINTER_BUTTON_STATE_RELEASED) return;
  if (client->pointerSurface == SurfaceKind::TopBar && client->pointerX < 88) openLauncher(*client);
  else if (client->pointerSurface == SurfaceKind::Dock) handleDockClick(*client);
  else if (client->pointerSurface == SurfaceKind::Launcher) {
    auto results = launcherResults(*client);
    if (auto index = launcherResultAt(*client, results)) {
      activateResult(*client, results[*index]);
    } else if (!launcherPointerInsideContent(*client, results)) {
      closeLauncher(*client);
    }
  }
  requestRender(*client, client->pointerSurface);
}

void pointerAxis(void*, wl_pointer*, std::uint32_t, std::uint32_t, wl_fixed_t) {}
void pointerFrame(void*, wl_pointer*) {}
void pointerAxisSource(void*, wl_pointer*, std::uint32_t) {}
void pointerAxisStop(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
void pointerAxisDiscrete(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
wl_pointer_listener const kPointerListener{pointerEnter, pointerLeave, pointerMotion, pointerButton, pointerAxis,
                                           pointerFrame, pointerAxisSource, pointerAxisStop, pointerAxisDiscrete};

void keymap(void* data, wl_keyboard*, std::uint32_t format, int fd, std::uint32_t size) {
  auto* client = static_cast<ShellClient*>(data);
  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }
  char* map = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
  close(fd);
  if (map == MAP_FAILED) return;
  if (!client->xkbContext) client->xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (client->xkbKeymap) xkb_keymap_unref(client->xkbKeymap);
  if (client->xkbState) xkb_state_unref(client->xkbState);
  client->xkbKeymap = xkb_keymap_new_from_string(client->xkbContext, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  client->xkbState = client->xkbKeymap ? xkb_state_new(client->xkbKeymap) : nullptr;
  munmap(map, size);
}

void keyboardEnter(void*, wl_keyboard*, std::uint32_t, wl_surface*, wl_array*) {}
void keyboardLeave(void*, wl_keyboard*, std::uint32_t, wl_surface*) {}

void keyboardKey(void* data, wl_keyboard*, std::uint32_t, std::uint32_t, std::uint32_t key, std::uint32_t state) {
  auto* client = static_cast<ShellClient*>(data);
  if (!client->launcherOpen || state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
  if (key == KEY_ESC) {
    closeLauncher(*client);
    return;
  }
  if (key == KEY_BACKSPACE) {
    if (!client->query.empty()) client->query.pop_back();
    requestRender(*client, SurfaceKind::Launcher);
    return;
  }
  auto results = launcherResults(*client);
  if (key == KEY_DOWN) {
    if (!results.empty()) client->highlighted = std::min<int>(client->highlighted + 1, static_cast<int>(results.size()) - 1);
    requestRender(*client, SurfaceKind::Launcher);
    return;
  }
  if (key == KEY_UP) {
    client->highlighted = std::max(0, client->highlighted - 1);
    requestRender(*client, SurfaceKind::Launcher);
    return;
  }
  if (key == KEY_ENTER || key == KEY_KPENTER) {
    if (!results.empty()) activateResult(*client, results[std::clamp(client->highlighted, 0, static_cast<int>(results.size()) - 1)]);
    return;
  }
  if (!client->xkbState) return;
  xkb_state_update_key(client->xkbState, key + 8u, XKB_KEY_DOWN);
  char utf8[64]{};
  int const written = xkb_state_key_get_utf8(client->xkbState, key + 8u, utf8, sizeof(utf8));
  if (written > 0 && client->query.size() < 128u) {
    client->query.append(utf8, static_cast<std::size_t>(written));
    client->highlighted = 0;
    requestRender(*client, SurfaceKind::Launcher);
  }
}

void keyboardModifiers(void* data, wl_keyboard*, std::uint32_t, std::uint32_t depressed,
                       std::uint32_t latched, std::uint32_t locked, std::uint32_t group) {
  auto* client = static_cast<ShellClient*>(data);
  if (client->xkbState) xkb_state_update_mask(client->xkbState, depressed, latched, locked, 0, 0, group);
}

void keyboardRepeat(void*, wl_keyboard*, std::int32_t, std::int32_t) {}
wl_keyboard_listener const kKeyboardListener{keymap, keyboardEnter, keyboardLeave, keyboardKey, keyboardModifiers, keyboardRepeat};

void seatCapabilities(void* data, wl_seat* seat, std::uint32_t caps) {
  auto* client = static_cast<ShellClient*>(data);
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !client->pointer) {
    client->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(client->pointer, &kPointerListener, client);
  }
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !client->keyboard) {
    client->keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(client->keyboard, &kKeyboardListener, client);
  }
}

void seatName(void*, wl_seat*, char const*) {}
wl_seat_listener const kSeatListener{seatCapabilities, seatName};

void registryGlobal(void* data, wl_registry* registry, std::uint32_t name, char const* interface, std::uint32_t version) {
  auto* client = static_cast<ShellClient*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    client->compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    client->seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
    wl_seat_add_listener(client->seat, &kSeatListener, client);
  } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    client->layerShell = static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1));
  } else if (std::strcmp(interface, ext_background_effect_manager_v1_interface.name) == 0) {
    client->backgroundEffectManager = static_cast<ext_background_effect_manager_v1*>(
        wl_registry_bind(registry, name, &ext_background_effect_manager_v1_interface, 1));
  } else if (std::strcmp(interface, wp_viewporter_interface.name) == 0) {
    client->viewporter = static_cast<wp_viewporter*>(wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
  }
}

void registryRemove(void*, wl_registry*, std::uint32_t) {}
wl_registry_listener const kRegistryListener{registryGlobal, registryRemove};

void resetDock(ShellClient& client) {
  client.dockItems = {
      {"launcher", "launcher", "Launcher", {}, false, false, false},
      {"sep1", "separator", "", {}, false, false, false},
      {"files", "app", "Files", "files", false, false, false},
      {"browser", "app", "Browser", "browser", false, false, false},
      {"terminal", "app", "Terminal", "terminal", false, false, false},
      {"settings", "app", "Settings", "settings", false, false, false},
      {"calendar", "app", "Calendar", "calendar", false, false, false},
      {"mail", "app", "Mail", "mail", false, false, false},
      {"music", "app", "Music", "music", false, false, false},
      {"sep2", "separator", "", {}, false, false, false},
      {"trash", "trash", "Trash", "trash", false, false, true},
  };
}

void applySnapshot(ShellClient& client, std::string_view json) {
  for (auto& item : client.dockItems) {
    item.running = false;
    item.focused = false;
  }
  client.activeTitle.clear();
  for (auto& item : client.dockItems) {
    if (item.appId.empty()) continue;
    std::size_t search = 0;
    while (search < json.size()) {
      std::size_t const pos = json.find("\"appId\":\"", search);
      if (pos == std::string_view::npos) break;
      std::size_t const valueStart = pos + 9u;
      std::size_t const valueEnd = json.find('"', valueStart);
      if (valueEnd == std::string_view::npos) break;
      std::string_view actualAppId = json.substr(valueStart, valueEnd - valueStart);
      if (appIdMatches(item.appId, actualAppId)) {
        item.running = true;
        std::size_t const objectEnd = json.find('}', valueEnd);
        item.focused = objectEnd != std::string_view::npos &&
                       json.substr(valueEnd, objectEnd - valueEnd).find("\"focused\":true") != std::string_view::npos;
        if (item.focused) {
          client.activeTitle = jsonStringField(json, "title", pos);
          if (client.activeTitle.empty()) client.activeTitle = item.label;
        }
        break;
      }
      search = valueEnd + 1u;
    }
  }
  client.network = jsonStringField(json, "network");
  client.wifi = jsonStringField(json, "wifi");
  client.bluetooth = jsonStringField(json, "bluetooth");
  client.volume = jsonStringField(json, "volume");
  client.battery = jsonStringField(json, "battery");
  float const nextScale = jsonFloatField(json, "scale", client.outputScale);
  if (std::abs(nextScale - client.outputScale) > 0.001f) {
    client.outputScale = nextScale;
    for (ShellSurface* surface : {&client.topbar, &client.dock, &client.launcher}) {
      updateViewportDestination(client, *surface);
    }
  }
  requestRenderAll(client);
}

void handleIpcLine(ShellClient& client, std::string_view line) {
  if (line.find("\"lambda.shell.openCommandLauncher\"") != std::string_view::npos) {
    openLauncher(client);
  } else if (line.find("\"lambda.windowManager.snapshot\"") != std::string_view::npos) {
    applySnapshot(client, line);
  }
  renderAll(client);
}

int connectIpc() {
  std::string path;
  if (char const* env = std::getenv("LAMBDA_SHELL_SOCKET"); env && *env) path = env;
  else path = runtimePath("lambda-window-manager-shell.sock");
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return -1;
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

void dispatchIpc(ShellClient& client) {
  if (client.ipcFd < 0) return;
  char buffer[4096];
  ssize_t const n = read(client.ipcFd, buffer, sizeof(buffer));
  if (n <= 0) {
    if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) gRunning = false;
    return;
  }
  client.ipcBuffer.append(buffer, static_cast<std::size_t>(n));
  for (;;) {
    std::size_t const newline = client.ipcBuffer.find('\n');
    if (newline == std::string::npos) break;
    std::string line = client.ipcBuffer.substr(0, newline);
    client.ipcBuffer.erase(0, newline + 1u);
    handleIpcLine(client, line);
  }
}

} // namespace

int main() {
  ShellClient client;
  try {
    resetDock(client);
    std::string const display = displayName();
    client.display = wl_display_connect(display.empty() ? nullptr : display.c_str());
    if (!client.display) throw std::runtime_error("wl_display_connect failed");
    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    wl_display_roundtrip(client.display);
    if (!client.compositor || !client.layerShell) {
      throw std::runtime_error("lambda-shell requires wl_compositor and zwlr_layer_shell_v1");
    }
    client.ipcFd = connectIpc();
    if (client.ipcFd < 0) throw std::runtime_error("failed to connect lambda-window-manager shell IPC");
    sendIpc(client, "{\"type\":\"lambda.shell.hello\",\"protocolVersion\":1,\"shellVersion\":\"0.1.0\",\"capabilities\":[\"topbar\",\"dock\",\"command-launcher\"]}");

    createLayer(client, client.topbar, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "lambda.topbar");
    zwlr_layer_surface_v1_set_size(client.topbar.layer, 0, kTopBarHeight);
    zwlr_layer_surface_v1_set_anchor(client.topbar.layer,
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(client.topbar.layer, kTopBarHeight);
    wl_surface_commit(client.topbar.surface);

    client.dock.width = dockWidth(client);
    client.dock.height = kDockPaddingY * 2 + kDockCell + 8;
    createLayer(client, client.dock, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "lambda.dock");
    zwlr_layer_surface_v1_set_size(client.dock.layer,
                                   static_cast<std::uint32_t>(client.dock.width),
                                   static_cast<std::uint32_t>(client.dock.height));
    zwlr_layer_surface_v1_set_anchor(client.dock.layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_margin(client.dock.layer, 0, 0, kDockBottom, 0);
    wl_surface_commit(client.dock.surface);

    wl_display_roundtrip(client.display);
    renderAll(client);

    while (gRunning.load(std::memory_order_relaxed)) {
      pollfd fds[2]{
          {.fd = wl_display_get_fd(client.display), .events = POLLIN, .revents = 0},
          {.fd = client.ipcFd, .events = POLLIN, .revents = 0},
      };
      int const timeout = 500;
      int const ready = poll(fds, 2, timeout);
      if (ready < 0 && errno == EINTR) continue;
      if (ready < 0) break;
      if (ready == 0) requestRender(client, SurfaceKind::TopBar);
      if (fds[0].revents) {
        if (wl_display_dispatch(client.display) < 0) break;
      } else {
        wl_display_dispatch_pending(client.display);
      }
      if (fds[1].revents) dispatchIpc(client);
      renderAll(client);
    }
    closeLauncher(client);
    destroySurface(client.launcher);
    destroySurface(client.dock);
    destroySurface(client.topbar);
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "lambda-shell: %s\n", e.what());
    return 1;
  }
}
