#pragma once

#include "../Common.hpp"
#include "../Constants.hpp"
#include "../Helpers.hpp"

namespace car {

struct MapPalette {
    Color land;
    Color water;
    Color park;
    Color road;
    Color minor;
    Color stroke;
};

inline constexpr MapPalette kMapPaletteNight{
    .land = Color::hex(0x1B1D24),
    .water = Color::hex(0x0E1620),
    .park = Color::hex(0x1F2A1E),
    .road = Color::hex(0x2A2E3A),
    .minor = Color::hex(0x232734),
    .stroke = Color::hex(0x14171C),
};

struct StylizedMap : ViewModifiers<StylizedMap> {
    float scale = 1.f;

    static StrokeStyle roundStroke(Color color, float width) {
        auto stroke = StrokeStyle::solid(color, width);
        stroke.cap = StrokeCap::Round;
        stroke.join = StrokeJoin::Round;
        return stroke;
    }

    static void drawMapRect(Canvas& canvas, float x, float y, float w, float h, Color color, float radius = 4.f) {
        canvas.drawRect(Rect{x, y, w, h}, CornerRadius{radius}, FillStyle::solid(color), StrokeStyle::none());
    }

    static void drawMapRoad(Canvas& canvas, float x, float y, float w, float h, Color color, float radius = 4.f) {
        drawMapRect(canvas, x, y, w, h, color, radius);
    }

    static void drawMapContent(Canvas& canvas, Rect frame) {
        constexpr MapPalette p = kMapPaletteNight;
        constexpr float baseW = 600.f;
        constexpr float baseH = 320.f;
        if (frame.width <= 0.f || frame.height <= 0.f) {
            return;
        }

        canvas.save();
        canvas.clipRect(frame, CornerRadius{}, true);
        canvas.translate(frame.x, frame.y);
        canvas.scale(frame.width / baseW, frame.height / baseH);

        drawMapRect(canvas, 0.f, 0.f, baseW, baseH, p.land, 0.f);
        drawMapRect(canvas, -30.f, 210.f, 660.f, 130.f, p.water, 65.f);
        drawMapRect(canvas, 340.f, 36.f, 135.f, 90.f, p.park, 18.f);
        drawMapRoad(canvas, 0.f, 82.f, baseW, 14.f, p.minor);
        drawMapRoad(canvas, 0.f, 258.f, baseW, 14.f, p.minor);
        drawMapRoad(canvas, 122.f, 0.f, 14.f, baseH, p.minor);
        drawMapRoad(canvas, 462.f, 0.f, 14.f, baseH, p.minor);
        drawMapRoad(canvas, 244.f, 0.f, 14.f, 204.f, p.minor);
        drawMapRoad(canvas, 0.f, 150.f, 620.f, 22.f, p.road, 9.f);
        drawMapRoad(canvas, 332.f, 116.f, 228.f, 10.f, p.minor);
        drawMapRoad(canvas, 352.f, 126.f, 10.f, 122.f, p.minor);

        Path route;
        route.moveTo({58.f, 290.f});
        route.lineTo({58.f, 174.f});
        route.bezierTo({93.f, 158.f}, {148.f, 168.f}, {206.f, 158.f});
        route.bezierTo({288.f, 144.f}, {341.f, 124.f}, {418.f, 76.f});
        canvas.drawPath(route, FillStyle::none(), roundStroke(alphaBlue(0.32f), 15.f));
        canvas.drawPath(route, FillStyle::none(), roundStroke(Color::hex(kSignatureBlue), 7.f));
        canvas.drawPath(route, FillStyle::none(), roundStroke(Color{0.70f, 0.88f, 1.f, 0.9f}, 2.f));

        canvas.drawCircle({58.f, 290.f}, 8.f, FillStyle::solid(p.land), StrokeStyle::solid(Color::hex(kSignatureBlue), 3.f));
        canvas.drawCircle({206.f, 158.f}, 25.f, FillStyle::solid(alphaBlue(0.15f)), StrokeStyle::none());
        canvas.drawCircle({206.f, 158.f}, 8.f, FillStyle::solid(Color::hex(kSignatureBlue)), StrokeStyle::solid(Colors::white, 2.5f));
        drawMapRect(canvas, 406.f, 58.f, 28.f, 36.f, Color::hex(kSignatureBlue), 14.f);
        canvas.drawCircle({420.f, 72.f}, 4.f, FillStyle::solid(Colors::white), StrokeStyle::none());

        canvas.restore();
    }

    auto body() const {
        float const preferredW = 600.f * scale;
        float const preferredH = 320.f * scale;
        return Render{
            .measureFn = [preferredW, preferredH](LayoutConstraints const& constraints, LayoutHints const&) {
                Size size{preferredW, preferredH};
                if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
                    size.width = constraints.maxWidth;
                }
                if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
                    size.height = constraints.maxHeight;
                }
                size.width = std::max(size.width, constraints.minWidth);
                size.height = std::max(size.height, constraints.minHeight);
                return size;
            },
            .draw = [](Canvas& canvas, Rect frame) { drawMapContent(canvas, frame); },
            .pure = true,
        };
    }
};

inline Element makeStylizedMapElement(float scale = 1.f) { return StylizedMap{.scale = scale}; }

} // namespace car
