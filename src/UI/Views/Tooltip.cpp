#include <Lambda/UI/Views/Tooltip.hpp>

#include <utility>

namespace lambda {

void useTooltip(TooltipConfig const& config) {
  (void)config;
}

void useTooltip(std::string text) {
  useTooltip(TooltipConfig {.text = std::move(text)});
}

} // namespace lambda
