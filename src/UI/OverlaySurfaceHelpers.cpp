#include <Flux/UI/OverlaySurfaceHelpers.hpp>

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Theme.hpp>

namespace flux {

ResolvedAlertCardColors resolveAlertCardColors(Color cardColor, Color cardStrokeColor, float cornerRadius,
                                               FluxTheme const& theme) {
  return ResolvedAlertCardColors{.cardFill = resolveColor(cardColor, theme.colorSurface),
                                 .cardStroke = resolveColor(cardStrokeColor, theme.colorBorderSubtle),
                                 .cornerRadius = CornerRadius{resolveFloat(cornerRadius, theme.radiusXLarge)}};
}

Color resolveAlertBackdropColor(Color backdropColor, FluxTheme const& theme) {
  return resolveColor(backdropColor, theme.colorScrimModal);
}

ResolvedPopoverCardBody resolvePopoverCardBody(Color backgroundColor, Color borderColor, float borderWidth,
                                               float cornerRadius, float contentPadding,
                                               FluxTheme const& theme) {
  return ResolvedPopoverCardBody{
      .background = resolveColor(backgroundColor, theme.colorSurfaceOverlay),
      .border = resolveColor(borderColor, theme.colorBorderSubtle),
      .borderWidth = borderWidth,
      .cornerRadius = CornerRadius{resolveFloat(cornerRadius, theme.radiusLarge)},
      .contentPadding = resolveFloat(contentPadding, theme.space3),
  };
}

Color resolvePopoverBackdropColor(Color backdropColor, FluxTheme const& theme) {
  return resolveColor(backdropColor, theme.colorScrimPopover);
}

} // namespace flux
