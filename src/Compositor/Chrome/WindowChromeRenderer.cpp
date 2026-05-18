#include "Compositor/Chrome/WindowChromeRenderer.hpp"

#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <algorithm>

namespace flux::compositor {
namespace {

constexpr float kWindowCornerRadius = 10.f;
constexpr float kTitleBarButtonSize = 12.f;
constexpr float kTitleBarButtonInset = 11.f;
constexpr float kTitleBarButtonGap = 8.f;

ShadowStyle macWindowShadow(bool focused) {
  return ShadowStyle{
      .radius = focused ? 26.f : 18.f,
      .offset = {0.f, focused ? 14.f : 9.f},
      .color = focused ? Color{0.f, 0.f, 0.f, 0.36f}
                       : Color{0.f, 0.f, 0.f, 0.20f},
  };
}

} // namespace

void drawWindowChrome(Canvas& canvas, TextSystem& textSystem, CommittedSurfaceSnapshot const& surface) {
  float const windowX = static_cast<float>(surface.x);
  float const windowY = static_cast<float>(surface.y);
  float const windowWidth = static_cast<float>(surface.width);
  float const windowHeight = static_cast<float>(surface.height);
  float const titleBarHeight = static_cast<float>(surface.titleBarHeight);
  if (titleBarHeight <= 0.f) return;

  Rect const frameRect = Rect::sharp(windowX,
                                     windowY - titleBarHeight,
                                     windowWidth,
                                     windowHeight + titleBarHeight);
  CornerRadius const frameRadius{kWindowCornerRadius};
  canvas.drawRect(frameRect,
                  frameRadius,
                  FillStyle::solid(Color{0.95f, 0.95f, 0.96f, 1.f}),
                  StrokeStyle::none(),
                  macWindowShadow(surface.focused));

  Color const titleFill =
      surface.focused ? Color{0.88f, 0.88f, 0.89f, 1.f}
                      : Color{0.78f, 0.79f, 0.81f, 1.f};
  Color const borderColor =
      surface.focused ? Color{0.48f, 0.49f, 0.52f, 1.f}
                      : Color{0.58f, 0.59f, 0.62f, 1.f};
  canvas.drawRect(Rect::sharp(windowX, windowY - titleBarHeight, windowWidth, titleBarHeight),
                  CornerRadius{kWindowCornerRadius, kWindowCornerRadius, 0.f, 0.f},
                  FillStyle::solid(titleFill),
                  StrokeStyle::none(),
                  ShadowStyle::none());
  canvas.drawRect(frameRect,
                  frameRadius,
                  FillStyle::none(),
                  StrokeStyle::solid(borderColor, 1.f),
                  ShadowStyle::none());

  float const buttonY = windowY - titleBarHeight + (titleBarHeight - kTitleBarButtonSize) * 0.5f;
  float const closeX = windowX + kTitleBarButtonInset;
  float const minimizeX = closeX + kTitleBarButtonSize + kTitleBarButtonGap;
  float const zoomX = minimizeX + kTitleBarButtonSize + kTitleBarButtonGap;
  auto drawTrafficLight = [&](float x, Color activeColor) {
    Color const fill = surface.focused ? activeColor : Color{0.62f, 0.63f, 0.65f, 1.f};
    canvas.drawCircle({x + kTitleBarButtonSize * 0.5f, buttonY + kTitleBarButtonSize * 0.5f},
                      kTitleBarButtonSize * 0.5f,
                      FillStyle::solid(fill),
                      StrokeStyle::solid(Color{0.f, 0.f, 0.f, 0.14f}, 1.f));
  };
  drawTrafficLight(closeX, Color{1.00f, 0.37f, 0.32f, 1.f});
  drawTrafficLight(minimizeX, Color{1.00f, 0.75f, 0.20f, 1.f});
  drawTrafficLight(zoomX, Color{0.20f, 0.78f, 0.35f, 1.f});

  float const titleLeft = windowX + 92.f;
  float const titleWidth = std::max(0.f, windowWidth - 184.f);
  if (titleWidth > 0.f && !surface.title.empty()) {
    Font titleFont{};
    titleFont.size = 13.f;
    titleFont.weight = 500.f;
    TextLayoutOptions titleOptions{
        .horizontalAlignment = HorizontalAlignment::Center,
        .verticalAlignment = VerticalAlignment::Center,
        .wrapping = TextWrapping::NoWrap,
        .maxLines = 1,
    };
    Color const titleColor =
        surface.focused ? Color{0.18f, 0.19f, 0.21f, 1.f}
                        : Color{0.42f, 0.43f, 0.46f, 1.f};
    auto titleLayout =
        textSystem.layout(surface.title,
                          titleFont,
                          titleColor,
                          Rect::sharp(titleLeft,
                                      windowY - titleBarHeight,
                                      titleWidth,
                                      titleBarHeight),
                          titleOptions);
    if (titleLayout) {
      canvas.save();
      canvas.clipRect(Rect::sharp(titleLeft,
                                  windowY - titleBarHeight,
                                  titleWidth,
                                  titleBarHeight));
      canvas.drawTextLayout(*titleLayout, {0.f, 0.f});
      canvas.restore();
    }
  }
}

void drawSnapPreview(Canvas& canvas, SnapPreviewSnapshot const& preview) {
  Rect const previewRect = Rect::sharp(static_cast<float>(preview.x),
                                      static_cast<float>(preview.y),
                                      static_cast<float>(preview.width),
                                      static_cast<float>(preview.height));
  canvas.drawRect(previewRect,
                  CornerRadius{0.f},
                  FillStyle::solid(Color{0.86f, 0.93f, 1.0f, 0.22f}),
                  StrokeStyle::solid(Color{0.92f, 0.97f, 1.0f, 0.82f}, 2.f),
                  ShadowStyle::none());
}

void drawCommandLauncher(Canvas& canvas,
                         TextSystem& textSystem,
                         CommandLauncherSnapshot const& launcher,
                         std::int32_t outputWidth,
                         std::int32_t outputHeight) {
  if (!launcher.visible) return;
  float const width = std::min(680.f, std::max(280.f, static_cast<float>(outputWidth) - 80.f));
  float const height = 78.f;
  float const x = (static_cast<float>(outputWidth) - width) * 0.5f;
  float const y = std::max(34.f, static_cast<float>(outputHeight) * 0.18f);
  Rect const panel = Rect::sharp(x, y, width, height);
  CornerRadius const radius{16.f};

  canvas.drawRect(panel,
                  radius,
                  FillStyle::solid(Color{0.94f, 0.95f, 0.97f, 0.96f}),
                  StrokeStyle::solid(Color{0.62f, 0.64f, 0.68f, 0.85f}, 1.f),
                  ShadowStyle{.radius = 30.f, .offset = {0.f, 18.f}, .color = Color{0.f, 0.f, 0.f, 0.38f}});

  Font commandFont{};
  commandFont.size = 24.f;
  commandFont.weight = 480.f;
  TextLayoutOptions commandOptions{
      .verticalAlignment = VerticalAlignment::Center,
      .wrapping = TextWrapping::NoWrap,
      .maxLines = 1,
  };
  std::string command = launcher.command.empty() ? std::string{"Run command"} : launcher.command;
  Color commandColor = launcher.command.empty() ? Color{0.47f, 0.49f, 0.53f, 1.f}
                                                : Color{0.08f, 0.09f, 0.11f, 1.f};
  Rect const commandRect = Rect::sharp(x + 26.f, y + 12.f, width - 52.f, 38.f);
  if (auto layout = textSystem.layout(command, commandFont, commandColor, commandRect, commandOptions)) {
    canvas.save();
    canvas.clipRect(commandRect);
    canvas.drawTextLayout(*layout, {0.f, 0.f});
    canvas.restore();
  }

  Font hintFont{};
  hintFont.size = 12.f;
  hintFont.weight = 420.f;
  TextLayoutOptions hintOptions{
      .verticalAlignment = VerticalAlignment::Center,
      .wrapping = TextWrapping::NoWrap,
      .maxLines = 1,
  };
  std::string hint = launcher.message.empty() ? std::string{"Enter to run, Escape to cancel"} : launcher.message;
  Rect const hintRect = Rect::sharp(x + 28.f, y + 51.f, width - 56.f, 18.f);
  if (auto layout = textSystem.layout(hint, hintFont, Color{0.40f, 0.42f, 0.46f, 1.f}, hintRect, hintOptions)) {
    canvas.drawTextLayout(*layout, {0.f, 0.f});
  }
}

} // namespace flux::compositor
