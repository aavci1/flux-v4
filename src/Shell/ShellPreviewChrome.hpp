#pragma once

#include "Shell/UI/LambdaShellTypes.hpp"

#include <Flux/Core/Color.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/Views.hpp>
#include <Flux/UI/Window.hpp>

namespace lambda_shell {

/// Visual chrome drawn by the compositor for layer-shell surfaces in production.
/// The preview app draws these locally so shell UI can be tested without Wayland.
namespace shell_preview {

using flux::LayerShellChromeOptions;
using flux::LayerShellChromeStyle;

inline constexpr float kDockCornerRadius = 14.f;

inline LayerShellChromeOptions defaultTopBarChrome() {
  LayerShellChromeOptions chrome{};
  chrome.style = LayerShellChromeStyle::BlurPanelBorder;
  chrome.squareBottomCorners = true;
  return chrome;
}

inline LayerShellChromeOptions defaultDockChrome() {
  LayerShellChromeOptions chrome{};
  chrome.style = LayerShellChromeStyle::BlurPanelBorder;
  return chrome;
}

inline flux::Color chromeTintFill(LayerShellChromeOptions const& chrome) {
  flux::Color tint = chrome.tint;
  tint.a *= chrome.tintOpacity;
  return tint;
}

inline flux::Element backdropLayer(float width,
                                   float height,
                                   LayerShellChromeOptions const& chrome,
                                   flux::CornerRadius corners = {}) {
  return flux::BackdropBlur{
      .radius = chrome.blurRadius,
      .tint = chromeTintFill(chrome),
      .corners = corners,
  }.size(width, height);
}

inline flux::Element wrapTopBar(flux::Element content, float width) {
  float const height = static_cast<float>(kTopBarHeight);
  LayerShellChromeOptions const chrome = defaultTopBarChrome();
  return flux::ZStack{
      .horizontalAlignment = flux::Alignment::Stretch,
      .verticalAlignment = flux::Alignment::Stretch,
      .children = flux::children(
          backdropLayer(width, height, chrome),
          flux::Rectangle{}
              .size(width, 1.f)
              .position(0.f, height - 1.f)
              .fill(chrome.borderColor),
          std::move(content).size(width, height)),
  }.size(width, height);
}

inline flux::Element wrapDock(flux::Element content, float width, float height) {
  LayerShellChromeOptions const chrome = defaultDockChrome();
  flux::CornerRadius const corners{kDockCornerRadius};
  return flux::ZStack{
      .horizontalAlignment = flux::Alignment::Stretch,
      .verticalAlignment = flux::Alignment::Stretch,
      .children = flux::children(
          backdropLayer(width, height, chrome, corners),
          flux::Rectangle{}
              .size(width, height)
              .cornerRadius(kDockCornerRadius)
              .stroke(flux::StrokeStyle::solid(chrome.borderColor, 1.f)),
          std::move(content)),
  }.size(width, height);
}

} // namespace shell_preview
} // namespace lambda_shell
