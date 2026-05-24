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

inline LayerShellChromeOptions defaultTopBarChrome() {
  LayerShellChromeOptions chrome{};
  chrome.style = LayerShellChromeStyle::BlurPanelBorder;
  chrome.cornerRadius = flux::CornerRadius{};
  return chrome;
}

inline LayerShellChromeOptions defaultDockChrome() {
  LayerShellChromeOptions chrome{};
  chrome.style = LayerShellChromeStyle::BlurPanelBorder;
  return chrome;
}

inline flux::Color chromeTintFill(LayerShellChromeOptions const& chrome) {
  flux::Color tint = chrome.glass.tintColor;
  tint.a *= chrome.glass.opacity;
  return tint;
}

inline flux::Color chromeBaseFill(LayerShellChromeOptions const& chrome) {
  flux::Color base = chrome.glass.baseColor;
  base.a *= chrome.glass.opacity;
  return base;
}

inline flux::Element backdropLayer(float width,
                                   float height,
                                   LayerShellChromeOptions const& chrome,
                                   flux::CornerRadius corners = {}) {
  return flux::ZStack{
      .horizontalAlignment = flux::Alignment::Stretch,
      .verticalAlignment = flux::Alignment::Stretch,
      .children = flux::children(
          flux::BackdropBlur{
              .radius = chrome.glass.blurRadius,
              .tint = chromeBaseFill(chrome),
              .corners = corners,
          }.size(width, height),
          flux::Rectangle{}
              .size(width, height)
              .fill(chromeTintFill(chrome))
              .cornerRadius(corners)),
  }.size(width, height);
}

inline flux::Element wrapTopBar(flux::Element content, float width) {
  float const height = static_cast<float>(kTopBarHeight);
  LayerShellChromeOptions const chrome = defaultTopBarChrome();
  return flux::ZStack{
      .horizontalAlignment = flux::Alignment::Stretch,
      .verticalAlignment = flux::Alignment::Stretch,
      .children = flux::children(
          backdropLayer(width, height, chrome, chrome.cornerRadius),
          flux::Rectangle{}
              .size(width, 1.f)
              .position(0.f, height - 1.f)
              .fill(chrome.glass.borderColor),
          std::move(content).size(width, height)),
  }.size(width, height);
}

inline flux::Element wrapDock(flux::Element content, float width, float height) {
  LayerShellChromeOptions const chrome = defaultDockChrome();
  return flux::ZStack{
      .horizontalAlignment = flux::Alignment::Stretch,
      .verticalAlignment = flux::Alignment::Stretch,
      .children = flux::children(
          backdropLayer(width, height, chrome, chrome.cornerRadius),
          flux::Rectangle{}
              .size(width, height)
              .cornerRadius(chrome.cornerRadius)
              .stroke(flux::StrokeStyle::solid(chrome.glass.borderColor, 1.f)),
          std::move(content)),
  }.size(width, height);
}

} // namespace shell_preview
} // namespace lambda_shell
