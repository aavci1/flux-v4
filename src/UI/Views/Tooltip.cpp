#include <Flux/UI/Views/Tooltip.hpp>

#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Popover.hpp>
#include <Flux/UI/Views/Text.hpp>

#include <utility>

namespace flux {

void useTooltip(TooltipConfig const& config) {
  auto [showPopover, hidePopover, presented] = usePopover();
  bool const hovered = useHover();
  if (hovered && !presented && !config.text.empty()) {
    showPopover(Popover {
        .content = Element {Text {
            .text = config.text,
            .font = Font::footnote(),
            .color = Color::primary(),
            .wrapping = TextWrapping::Wrap,
        }},
        .placement = config.placement,
        .gap = 6.f,
        .arrow = false,
        .backgroundColor = Color::elevatedBackground(),
        .borderColor = Color::separator(),
        .borderWidth = 1.f,
        .cornerRadius = 8.f,
        .contentPadding = 8.f,
        .maxSize = Size {240.f, 0.f},
        .backdropColor = Colors::transparent,
        .dismissOnEscape = true,
        .dismissOnOutsideTap = false,
        .useTapAnchor = false,
        .useHoverLeafAnchor = true,
        .debugName = "tooltip",
    });
  } else if (!hovered && presented) {
    hidePopover();
  }
}

void useTooltip(std::string text) {
  useTooltip(TooltipConfig {.text = std::move(text)});
}

} // namespace flux
