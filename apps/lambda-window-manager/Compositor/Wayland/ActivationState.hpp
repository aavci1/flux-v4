#pragma once

#include "Compositor/Wayland/WaylandServerImpl.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace lambda::compositor {

[[nodiscard]] inline bool activationTokenMutable(WaylandServer::Impl::ActivationToken const* token) {
  return token && !token->committed;
}

[[nodiscard]] inline bool activationTokenMatches(WaylandServer::Impl::ActivationToken const* token,
                                                 std::string_view tokenName) {
  return token && token->committed && token->token == tokenName;
}

[[nodiscard]] inline WaylandServer::Impl::ActivationToken* activationTokenForName(
    std::vector<std::unique_ptr<WaylandServer::Impl::ActivationToken>> const& tokens,
    std::string_view tokenName) {
  for (auto const& token : tokens) {
    if (activationTokenMatches(token.get(), tokenName)) return token.get();
  }
  return nullptr;
}

} // namespace lambda::compositor
