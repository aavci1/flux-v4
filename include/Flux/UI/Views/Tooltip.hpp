#pragma once

/// \file Flux/UI/Views/Tooltip.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Views/PopoverPlacement.hpp>

#include <string>

namespace flux {

struct TooltipConfig {
  std::string text;
  PopoverPlacement placement = PopoverPlacement::Above;
  int delayMs = 0;
};

void useTooltip(TooltipConfig const& config);
void useTooltip(std::string text);

} // namespace flux
