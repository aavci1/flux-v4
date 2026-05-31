#include "Compositor/Wayland/Globals/Activation.hpp"

#include "Compositor/Wayland/ActivationState.hpp"
#include "Compositor/Wayland/ResourceTemplates.hpp"
#include "Compositor/Wayland/WaylandServerImpl.hpp"
#include "xdg-activation-v1-server-protocol.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <wayland-server-core.h>

namespace lambda::compositor {

void focusSurface(WaylandServer::Impl* server, WaylandServer::Impl::Surface* surface, std::uint32_t timeMs);

namespace {

std::uint32_t monotonicMilliseconds() {
  using Clock = std::chrono::steady_clock;
  auto const now = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch());
  return static_cast<std::uint32_t>(now.count());
}

void activationTokenDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

bool postActivationTokenAlreadyUsedIfNeeded(wl_resource* resource,
                                            WaylandServer::Impl::ActivationToken const* token) {
  if (activationTokenMutable(token)) return false;
  wl_resource_post_error(resource,
                         XDG_ACTIVATION_TOKEN_V1_ERROR_ALREADY_USED,
                         "activation token already committed");
  return true;
}

void activationTokenSetSerial(wl_client*, wl_resource* resource, std::uint32_t serial, wl_resource*) {
  auto* token = resourceData<WaylandServer::Impl::ActivationToken>(resource);
  if (postActivationTokenAlreadyUsedIfNeeded(resource, token)) return;
  token->serial = serial;
}

void activationTokenSetAppId(wl_client*, wl_resource* resource, char const* appId) {
  auto* token = resourceData<WaylandServer::Impl::ActivationToken>(resource);
  if (postActivationTokenAlreadyUsedIfNeeded(resource, token)) return;
  token->appId = appId ? appId : "";
}

void activationTokenSetSurface(wl_client*, wl_resource* resource, wl_resource* surfaceResource) {
  auto* token = resourceData<WaylandServer::Impl::ActivationToken>(resource);
  if (postActivationTokenAlreadyUsedIfNeeded(resource, token)) return;
  token->surface = resourceData<WaylandServer::Impl::Surface>(surfaceResource);
}

void activationTokenCommit(wl_client*, wl_resource* resource) {
  auto* token = resourceData<WaylandServer::Impl::ActivationToken>(resource);
  if (postActivationTokenAlreadyUsedIfNeeded(resource, token)) return;
  token->committed = true;
  xdg_activation_token_v1_send_done(resource, token->token.c_str());
  wl_resource_set_user_data(resource, nullptr);
  token->resource = nullptr;
}

struct xdg_activation_token_v1_interface const activationTokenImpl{
    .set_serial = activationTokenSetSerial,
    .set_app_id = activationTokenSetAppId,
    .set_surface = activationTokenSetSurface,
    .commit = activationTokenCommit,
    .destroy = activationTokenDestroy,
};

void activationDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void activationGetToken(wl_client* client, wl_resource* resource, std::uint32_t id) {
  auto* server = serverFrom(resource);
  auto token = std::make_unique<WaylandServer::Impl::ActivationToken>();
  token->server = server;
  token->token = "lambda-" + std::to_string(server->nextActivationTokenId_++);
  wl_resource* tokenResource =
      wl_resource_create(client, &xdg_activation_token_v1_interface, wl_resource_get_version(resource), id);
  if (!tokenResource) {
    wl_client_post_no_memory(client);
    return;
  }
  token->resource = tokenResource;
  auto* raw = token.get();
  server->activationTokens_.push_back(std::move(token));
  wl_resource_set_implementation(tokenResource,
                                 &activationTokenImpl,
                                 raw,
                                 destroyResourceCallback<WaylandServer::Impl::ActivationToken,
                                                         WaylandServer::Impl,
                                                         &WaylandServer::Impl::destroyActivationToken>);
}

void activationActivate(wl_client*, wl_resource* resource, char const* tokenName, wl_resource* surfaceResource) {
  auto* server = serverFrom(resource);
  auto* surface = resourceData<WaylandServer::Impl::Surface>(surfaceResource);
  if (!server || !surfaceIsXdgToplevel(surface)) return;
  auto* token = activationTokenForName(server->activationTokens_, tokenName ? tokenName : "");
  if (!token) return;

  std::uint32_t const now = monotonicMilliseconds();
  server->lastActivationSurface_ = surface;
  server->lastActivationTimeMs_ = now;
  focusSurface(server, surface, now);
  server->destroyActivationToken(token);
  server->flushClients();
}

struct xdg_activation_v1_interface const activationImpl{
    .destroy = activationDestroy,
    .get_activation_token = activationGetToken,
    .activate = activationActivate,
};

} // namespace

void bindActivation(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &xdg_activation_v1_interface, std::min(version, 1u), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &activationImpl, data, nullptr);
}

} // namespace lambda::compositor
