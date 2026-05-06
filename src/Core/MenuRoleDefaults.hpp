#pragma once

#include <Flux/Core/MenuItem.hpp>
#include <Flux/Core/Shortcut.hpp>

#include <string>

namespace flux::detail {

std::string standardRoleActionName(MenuRole role);
Shortcut standardRoleShortcut(MenuRole role);

} // namespace flux::detail
