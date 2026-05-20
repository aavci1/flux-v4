#include "Compositor/Wayland/Globals/BackgroundEffect.hpp"

#include "Compositor/Wayland/ResourceTemplates.hpp"
#include "Compositor/Wayland/WaylandServerImpl.hpp"
#include "ext-background-effect-v1-server-protocol.h"

#include <algorithm>
#include <memory>
#include <vector>
#include <wayland-server-core.h>

namespace flux::compositor {
namespace {

void backgroundEffectManagerDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void backgroundEffectSurfaceDestroy(wl_client*, wl_resource* resource) {
  auto* effect = resourceData<WaylandServer::Impl::BackgroundEffect>(resource);
  if (effect && effect->surface) {
    effect->surface->pendingBackgroundBlurRects.clear();
    effect->surface->backgroundBlurPending = true;
  }
  wl_resource_destroy(resource);
}

std::vector<CommittedSurfaceSnapshot::RegionRect> copyRegionRects(wl_resource* regionResource) {
  if (!regionResource) return {};
  auto* region = resourceData<WaylandServer::Impl::Region>(regionResource);
  return region ? region->rects : std::vector<CommittedSurfaceSnapshot::RegionRect>{};
}

void backgroundEffectSurfaceSetBlurRegion(wl_client*, wl_resource* resource, wl_resource* regionResource) {
  auto* effect = resourceData<WaylandServer::Impl::BackgroundEffect>(resource);
  if (!effect || !effect->surface) {
    wl_resource_post_error(resource,
                           EXT_BACKGROUND_EFFECT_SURFACE_V1_ERROR_SURFACE_DESTROYED,
                           "associated wl_surface has been destroyed");
    return;
  }

  effect->surface->pendingBackgroundBlurRects = copyRegionRects(regionResource);
  effect->surface->backgroundBlurPending = true;
}

struct ext_background_effect_surface_v1_interface const backgroundEffectSurfaceImpl{
    .destroy = backgroundEffectSurfaceDestroy,
    .set_blur_region = backgroundEffectSurfaceSetBlurRegion,
};

void backgroundEffectManagerGetBackgroundEffect(wl_client* client,
                                                wl_resource* resource,
                                                std::uint32_t id,
                                                wl_resource* surfaceResource) {
  auto* server = serverFrom(resource);
  auto* surface = resourceData<WaylandServer::Impl::Surface>(surfaceResource);
  if (!server || !surface) return;
  if (surface->backgroundEffect) {
    wl_resource_post_error(resource,
                           EXT_BACKGROUND_EFFECT_MANAGER_V1_ERROR_BACKGROUND_EFFECT_EXISTS,
                           "wl_surface already has a background effect object");
    return;
  }

  auto effect = std::make_unique<WaylandServer::Impl::BackgroundEffect>();
  effect->server = server;
  effect->surface = surface;
  wl_resource* effectResource = wl_resource_create(client, &ext_background_effect_surface_v1_interface, 1, id);
  if (!effectResource) {
    wl_client_post_no_memory(client);
    return;
  }

  effect->resource = effectResource;
  auto* raw = effect.get();
  surface->backgroundEffect = raw;
  server->backgroundEffects_.push_back(std::move(effect));
  wl_resource_set_implementation(effectResource,
                                 &backgroundEffectSurfaceImpl,
                                 raw,
                                 destroyResourceCallback<WaylandServer::Impl::BackgroundEffect,
                                                         WaylandServer::Impl,
                                                         &WaylandServer::Impl::destroyBackgroundEffect>);
}

struct ext_background_effect_manager_v1_interface const backgroundEffectManagerImpl{
    .destroy = backgroundEffectManagerDestroy,
    .get_background_effect = backgroundEffectManagerGetBackgroundEffect,
};

} // namespace

void bindBackgroundEffectManager(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &ext_background_effect_manager_v1_interface, std::min(version, 1u), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &backgroundEffectManagerImpl, data, nullptr);
  ext_background_effect_manager_v1_send_capabilities(
      resource,
      EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR);
}

} // namespace flux::compositor
