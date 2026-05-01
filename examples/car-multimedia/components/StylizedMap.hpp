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
    .park = Color::hex(0x1F2620),
    .road = Color::hex(0x2E323B),
    .minor = Color::hex(0x23262D),
    .stroke = Color::hex(0x15171C),
};

struct StylizedMap : ViewModifiers<StylizedMap> {
    float scale = 1.f;

    static StrokeStyle roundStroke(Color color, float width) {
        auto stroke = StrokeStyle::solid(color, width);
        stroke.cap = StrokeCap::Round;
        stroke.join = StrokeJoin::Round;
        return stroke;
    }

    auto body() const {
        constexpr MapPalette p = kMapPaletteNight;
        auto ring = useAnimation<float>(0.f);
        if (!ring.isRunning() && std::abs(*ring) < 0.001f) {
            ring.play(1.f, AnimationOptions{
                .transition = Transition::ease(2.4f),
                .repeat = AnimationOptions::kRepeatForever,
                .autoreverse = true,
            });
        }

        Color const accent = Color::hex(kSignatureBlue);
        Reactive::Bindable<float> ringRadius{[ring] { return 16.f + ring() * 10.f; }};
        Reactive::Bindable<FillStyle> ringFill{[ring] {
            return FillStyle::solid(alphaBlue(0.28f - ring() * 0.23f));
        }};

        return Svg{
            .viewBox = Rect{0.f, 0.f, 600.f, 320.f},
            .preserveAspectRatio = SvgPreserveAspectRatio::Slice,
            .intrinsicSize = Size{600.f * scale, 320.f * scale},
            .root = SvgGroup{.children = {
                SvgRect{.x = 0.f, .y = 0.f, .width = 600.f, .height = 320.f, .fill = FillStyle::solid(p.land)},
                SvgPath{
                    .d = "M -20,210 C 80,180 180,250 280,210 S 480,170 620,200 L 620,340 L -20,340 Z",
                    .fill = FillStyle::solid(p.water),
                    .opacity = 0.9f,
                },
                SvgPath{.d = "M 340,40 L 460,30 L 480,110 L 380,130 Z", .fill = FillStyle::solid(p.park), .opacity = 0.9f},
                SvgPath{.d = "M 0,80 L 600,90", .stroke = StrokeStyle::solid(p.minor, 14.f)},
                SvgPath{.d = "M 0,260 L 600,270", .stroke = StrokeStyle::solid(p.minor, 14.f)},
                SvgPath{.d = "M 120,0 L 130,320", .stroke = StrokeStyle::solid(p.minor, 14.f)},
                SvgPath{.d = "M 460,0 L 470,320", .stroke = StrokeStyle::solid(p.minor, 14.f)},
                SvgPath{.d = "M 240,0 L 250,200", .stroke = StrokeStyle::solid(p.minor, 14.f)},
                SvgPath{.d = "M 0,160 C 100,150 180,180 280,150 S 480,140 620,170", .stroke = StrokeStyle::solid(p.road, 20.f)},
                SvgPath{.d = "M 0,160 C 100,150 180,180 280,150 S 480,140 620,170", .stroke = StrokeStyle::solid(p.stroke, 0.5f)},
                SvgPath{
                    .d = "M 60,290 L 60,170 C 60,162 64,158 72,158 L 280,150 C 350,148 380,120 420,80",
                    .stroke = roundStroke(accent, 6.f),
                },
                SvgPath{
                    .d = "M 60,290 L 60,170 C 60,162 64,158 72,158 L 280,150 C 350,148 380,120 420,80",
                    .stroke = StrokeStyle::solid(Colors::white, 2.f),
                    .opacity = 0.4f,
                },
                SvgCircle{.cx = 60.f, .cy = 290.f, .r = 8.f, .fill = FillStyle::solid(p.land), .stroke = StrokeStyle::solid(accent, 3.f)},
                SvgCircle{.cx = 200.f, .cy = 153.f, .r = ringRadius, .fill = ringFill},
                SvgCircle{.cx = 200.f, .cy = 153.f, .r = 7.f, .fill = FillStyle::solid(accent), .stroke = StrokeStyle::solid(Colors::white, 2.5f)},
                translated(420.f, 80.f, {
                    SvgPath{
                        .d = "M 0,-22 C -8,-22 -14,-16 -14,-8 C -14,2 0,14 0,14 C 0,14 14,2 14,-8 C 14,-16 8,-22 0,-22 Z",
                        .fill = FillStyle::solid(accent),
                    },
                    SvgCircle{.cx = 0.f, .cy = -9.f, .r = 4.f, .fill = FillStyle::solid(Colors::white)},
                }),
            }},
        };
    }
};

inline Element makeStylizedMapElement(float scale = 1.f) { return StylizedMap{.scale = scale}; }

} // namespace car
