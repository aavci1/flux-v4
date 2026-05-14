#pragma once

#include <Flux/UI/MenuItem.hpp>
#include <Flux/UI/Shortcut.hpp>

#include <string>

namespace flux::detail {

std::string standardRoleActionName(MenuRole role);
Shortcut standardRoleShortcut(MenuRole role);

} // namespace flux::detail
