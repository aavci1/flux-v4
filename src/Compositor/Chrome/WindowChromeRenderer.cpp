#include "Compositor/Chrome/WindowChromeRenderer.hpp"

#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <algorithm>

namespace flux::compositor {

void drawWindowChrome(Canvas& canvas, TextSystem& textSystem, CommittedSurfaceSnapshot const& surface) {
  float const windowX = static_cast<float>(surface.x);
  float const windowY = static_cast<float>(surface.y);
  float const windowWidth = static_cast<float>(surface.width);
  float const windowHeight = static_cast<float>(surface.height);
  float const titleBarHeight = static_cast<float>(surface.titleBarHeight);
  if (titleBarHeight <= 0.f) return;

  Color const titleFill =
      surface.focused ? Color{0.10f, 0.12f, 0.14f, 1.f}
                      : Color{0.30f, 0.32f, 0.36f, 1.f};
  Color const borderColor =
      surface.focused ? Color{0.02f, 0.03f, 0.04f, 1.f}
                      : Color{0.18f, 0.19f, 0.21f, 1.f};
  canvas.drawRect(Rect::sharp(windowX, windowY - titleBarHeight, windowWidth, titleBarHeight),
                  CornerRadius{0.f},
                  FillStyle::solid(titleFill),
                  StrokeStyle::none(),
                  ShadowStyle::none());
  canvas.drawRect(Rect::sharp(windowX - 1.f,
                              windowY - titleBarHeight - 1.f,
                              windowWidth + 2.f,
                              windowHeight + titleBarHeight + 2.f),
                  CornerRadius{0.f},
                  FillStyle::none(),
                  StrokeStyle::solid(borderColor, 1.f),
                  ShadowStyle::none());

  float constexpr closeSize = 18.f;
  float constexpr closeInset = 5.f;
  float const closeX = windowX + windowWidth - closeInset - closeSize;
  float const closeY = windowY - titleBarHeight + closeInset;
  Color const closeFill =
      surface.focused ? Color{0.86f, 0.20f, 0.22f, 1.f}
                      : Color{0.48f, 0.20f, 0.22f, 1.f};
  Color const closeStroke =
      surface.focused ? Color{0.98f, 0.88f, 0.88f, 1.f}
                      : Color{0.68f, 0.58f, 0.58f, 1.f};
  canvas.drawCircle({closeX + closeSize * 0.5f, closeY + closeSize * 0.5f},
                    closeSize * 0.5f,
                    FillStyle::solid(closeFill),
                    StrokeStyle::none());
  canvas.drawLine({closeX + 6.f, closeY + 6.f},
                  {closeX + closeSize - 6.f, closeY + closeSize - 6.f},
                  StrokeStyle::solid(closeStroke, 1.5f));
  canvas.drawLine({closeX + closeSize - 6.f, closeY + 6.f},
                  {closeX + 6.f, closeY + closeSize - 6.f},
                  StrokeStyle::solid(closeStroke, 1.5f));

  float const titleLeft = windowX + 10.f;
  float const titleWidth = std::max(0.f, closeX - titleLeft - 8.f);
  if (titleWidth > 0.f && !surface.title.empty()) {
    Font titleFont{};
    titleFont.size = 13.f;
    titleFont.weight = 500.f;
    TextLayoutOptions titleOptions{
        .verticalAlignment = VerticalAlignment::Center,
        .wrapping = TextWrapping::NoWrap,
        .maxLines = 1,
    };
    Color const titleColor =
        surface.focused ? Color{0.94f, 0.96f, 0.98f, 1.f}
                        : Color{0.72f, 0.75f, 0.80f, 1.f};
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

  Color const gripColor =
      surface.focused ? Color{0.78f, 0.82f, 0.88f, 1.f}
                      : Color{0.55f, 0.58f, 0.64f, 1.f};
  float constexpr grip = 10.f;
  float constexpr gripStroke = 2.f;
  auto drawGrip = [&](float x, float y) {
    canvas.drawRect(Rect::sharp(x, y, grip, gripStroke),
                    CornerRadius{0.f},
                    FillStyle::solid(gripColor),
                    StrokeStyle::none(),
                    ShadowStyle::none());
    canvas.drawRect(Rect::sharp(x, y, gripStroke, grip),
                    CornerRadius{0.f},
                    FillStyle::solid(gripColor),
                    StrokeStyle::none(),
                    ShadowStyle::none());
  };
  drawGrip(windowX + 3.f, windowY + 3.f);
  drawGrip(windowX + windowWidth - grip - 3.f, windowY + 3.f);
  drawGrip(windowX + 3.f, windowY + windowHeight - grip - 3.f);
  drawGrip(windowX + windowWidth - grip - 3.f, windowY + windowHeight - grip - 3.f);
}

} // namespace flux::compositor
