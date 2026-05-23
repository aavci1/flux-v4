#include "Compositor/Wayland/Globals/LayerChrome.hpp"

#include "Compositor/Wayland/ResourceTemplates.hpp"
#include "Compositor/Wayland/WaylandServerImpl.hpp"
#include "wlr-layer-shell-unstable-v1-server-protocol.h"
#include "xx-layer-chrome-v1-server-protocol.h"

#include <cmath>
#include <memory>
#include <wayland-server-core.h>
#include <wayland-util.h>

namespace flux::compositor {
namespace {

Color colorFromRgba(std::uint32_t rgba) {
  auto channel = [&](int shift) {
    return static_cast<float>((rgba >> shift) & 0xffu) / 255.f;
  };
  return Color{channel(24), channel(16), channel(8), channel(0)};
}

LayerShellChromeStyle styleFromProtocol(std::uint32_t style) {
  switch (style) {
  case XX_LAYER_CHROME_V1_STYLE_BLUR_PANEL:
    return LayerShellChromeStyle::BlurPanel;
  case XX_LAYER_CHROME_V1_STYLE_BLUR_PANEL_BORDER:
    return LayerShellChromeStyle::BlurPanelBorder;
  default:
    return LayerShellChromeStyle::None;
  }
}

void layerChromeDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void layerChromeSetStyle(wl_client*, wl_resource* resource, std::uint32_t style) {
  auto* chrome = resourceData<WaylandServer::Impl::LayerChrome>(resource);
  if (!chrome || !chrome->layerSurface) return;
  chrome->layerSurface->pendingChrome.style = styleFromProtocol(style);
  chrome->layerSurface->chromePending = true;
}

void layerChromeSetTint(wl_client*, wl_resource* resource, std::uint32_t rgba) {
  auto* chrome = resourceData<WaylandServer::Impl::LayerChrome>(resource);
  if (!chrome || !chrome->layerSurface) return;
  chrome->layerSurface->pendingChrome.tint = colorFromRgba(rgba);
  chrome->layerSurface->chromePending = true;
}

void layerChromeSetBorder(wl_client*, wl_resource* resource, std::uint32_t rgba) {
  auto* chrome = resourceData<WaylandServer::Impl::LayerChrome>(resource);
  if (!chrome || !chrome->layerSurface) return;
  chrome->layerSurface->pendingChrome.borderColor = colorFromRgba(rgba);
  chrome->layerSurface->chromePending = true;
}

void layerChromeSetBlurRadius(wl_client*, wl_resource* resource, std::int32_t radius) {
  auto* chrome = resourceData<WaylandServer::Impl::LayerChrome>(resource);
  if (!chrome || !chrome->layerSurface) return;
  chrome->layerSurface->pendingChrome.blurRadius =
      std::max(0.f, static_cast<float>(wl_fixed_to_double(radius)));
  chrome->layerSurface->chromePending = true;
}

void layerChromeSetSquareBottomCorners(wl_client*, wl_resource* resource, std::uint32_t enabled) {
  auto* chrome = resourceData<WaylandServer::Impl::LayerChrome>(resource);
  if (!chrome || !chrome->layerSurface) return;
  chrome->layerSurface->pendingChrome.squareBottomCorners = enabled != 0;
  chrome->layerSurface->chromePending = true;
}

struct xx_layer_chrome_v1_interface const layerChromeImpl{
    .destroy = layerChromeDestroy,
    .set_style = layerChromeSetStyle,
    .set_tint = layerChromeSetTint,
    .set_border = layerChromeSetBorder,
    .set_blur_radius = layerChromeSetBlurRadius,
    .set_square_bottom_corners = layerChromeSetSquareBottomCorners,
};

void layerChromeManagerDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void layerChromeManagerGetLayerChrome(wl_client* client,
                                      wl_resource* resource,
                                      std::uint32_t id,
                                      wl_resource* layerSurfaceResource) {
  auto* server = serverFrom(resource);
  auto* layerSurface = resourceData<WaylandServer::Impl::LayerSurface>(layerSurfaceResource);
  if (!layerSurface || !layerSurface->surface || !surfaceIsLayerSurface(layerSurface->surface)) {
    wl_resource_post_error(resource,
                           XX_LAYER_CHROME_MANAGER_V1_ERROR_INVALID_ROLE,
                           "xx_layer_chrome_v1 requires a zwlr_layer_surface_v1");
    return;
  }
  if (layerSurface->layerChrome) {
    wl_resource_post_error(resource,
                           XX_LAYER_CHROME_MANAGER_V1_ERROR_CHROME_EXISTS,
                           "layer surface already has chrome metadata");
    return;
  }

  auto chrome = std::make_unique<WaylandServer::Impl::LayerChrome>();
  chrome->server = server;
  chrome->layerSurface = layerSurface;
  wl_resource* chromeResource = wl_resource_create(client, &xx_layer_chrome_v1_interface, 1, id);
  if (!chromeResource) {
    wl_client_post_no_memory(client);
    return;
  }
  chrome->resource = chromeResource;
  auto* raw = chrome.get();
  layerSurface->layerChrome = raw;
  server->layerChromeObjects_.push_back(std::move(chrome));
  wl_resource_set_implementation(chromeResource,
                                 &layerChromeImpl,
                                 raw,
                                 destroyResourceCallback<WaylandServer::Impl::LayerChrome,
                                                         WaylandServer::Impl,
                                                         &WaylandServer::Impl::destroyLayerChrome>);
}

struct xx_layer_chrome_manager_v1_interface const layerChromeManagerImpl{
    .destroy = layerChromeManagerDestroy,
    .get_layer_chrome = layerChromeManagerGetLayerChrome,
};

} // namespace

bool applyLayerChromeState(WaylandServer::Impl::Surface* surface) {
  if (!surface || !surface->layerSurface || !surface->layerSurface->chromePending) return false;
  surface->layerSurface->chrome = surface->layerSurface->pendingChrome;
  surface->layerSurface->chromePending = false;
  return true;
}

void bindLayerChromeManager(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &xx_layer_chrome_manager_v1_interface, std::min(version, 1u), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &layerChromeManagerImpl, data, nullptr);
}

} // namespace flux::compositor
