#pragma once

#include "Compositor/WaylandServer.hpp"

#include <cstdint>

struct wl_client;

namespace flux::compositor {

void bindSeat(wl_client* client, void* data, std::uint32_t version, std::uint32_t id);
void sendKeyboardConfiguration(WaylandServer::Impl* server);

} // namespace flux::compositor
