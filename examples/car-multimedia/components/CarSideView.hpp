#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct CarSideView : ViewModifiers<CarSideView> {
    auto body() const {
        auto pulse = useAnimation<float>(0.f);
        if (!pulse.isRunning() && std::abs(*pulse) < 0.001f) {
            pulse.play(1.f, AnimationOptions{
                .transition = Transition::ease(2.6f),
                .repeat = AnimationOptions::kRepeatForever,
                .autoreverse = true,
            });
        }

        Reactive::Bindable<FillStyle> haloFill{[pulse] {
            return FillStyle::solid(alphaBlue(0.05f + pulse() * 0.20f));
        }};

        return Svg{
            .viewBox = Rect{0.f, 0.f, 360.f, 110.f},
            .preserveAspectRatio = SvgPreserveAspectRatio::Stretch,
            .root = SvgGroup{.children = {
                SvgPath{
                    .d = "M 30,80 C 30,72 36,66 44,66 L 76,66 L 100,38 C 108,30 122,26 138,26 L 232,26 C 254,26 274,34 290,50 L 312,66 L 332,68 C 340,68 346,74 346,82 L 346,86 C 346,90 342,94 338,94 L 296,94 A 24,24 0 0 0 248,94 L 130,94 A 24,24 0 0 0 82,94 L 36,94 C 32,94 28,90 28,86 Z",
                    .fill = FillStyle::solid(Color::elevatedBackground()),
                    .stroke = StrokeStyle::solid(Color::opaqueSeparator(), 1.5f),
                },
                SvgPath{.d = "M 110,42 L 132,30 L 184,30 L 188,64 L 110,64 Z", .fill = FillStyle::solid(Color::controlBackground()), .stroke = StrokeStyle::solid(Color::separator(), 1.f)},
                SvgPath{.d = "M 196,30 L 230,30 C 246,30 264,38 276,52 L 280,64 L 196,64 Z", .fill = FillStyle::solid(Color::controlBackground()), .stroke = StrokeStyle::solid(Color::separator(), 1.f)},
                SvgCircle{.cx = 106.f, .cy = 94.f, .r = 18.f, .fill = FillStyle::solid(Color::controlBackground()), .stroke = StrokeStyle::solid(Color::opaqueSeparator(), 1.5f)},
                SvgCircle{.cx = 106.f, .cy = 94.f, .r = 9.f, .fill = FillStyle::solid(Color::elevatedBackground())},
                SvgCircle{.cx = 272.f, .cy = 94.f, .r = 18.f, .fill = FillStyle::solid(Color::controlBackground()), .stroke = StrokeStyle::solid(Color::opaqueSeparator(), 1.5f)},
                SvgCircle{.cx = 272.f, .cy = 94.f, .r = 9.f, .fill = FillStyle::solid(Color::elevatedBackground())},
                SvgCircle{.cx = 60.f, .cy = 74.f, .r = 16.f, .fill = haloFill},
                SvgCircle{.cx = 60.f, .cy = 74.f, .r = 4.f, .fill = FillStyle::solid(Color::accent())},
            }},
        };
    }
};

inline Element makeCarSideViewElement() { return CarSideView{}; }

} // namespace car
