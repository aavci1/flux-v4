#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct CarTopdown : ViewModifiers<CarTopdown> {
    Reactive::Bindable<bool> flowFace{false};
    Reactive::Bindable<bool> flowFeet{false};
    Reactive::Bindable<bool> flowWind{false};

    auto body() const {
        auto pulse = useAnimation<float>(0.f);
        if (!pulse.isRunning() && std::abs(*pulse) < 0.001f) {
            pulse.play(1.f, AnimationOptions{
                .transition = Transition::ease(3.f),
                .repeat = AnimationOptions::kRepeatForever,
                .autoreverse = true,
            });
        }

        Reactive::Bindable<float> faceRadius{[pulse] { return 14.f + pulse() * 14.f; }};
        Reactive::Bindable<FillStyle> faceFill{[pulse] {
            return FillStyle::solid(alphaBlue(0.15f));
        }};

        return Svg{
            .viewBox = Rect{0.f, 0.f, 200.f, 140.f},
            .preserveAspectRatio = SvgPreserveAspectRatio::Stretch,
            .root = SvgGroup{.children = {
                SvgPath{
                    .d = "M 60,12 C 50,12 42,18 40,30 L 36,60 C 34,72 34,84 36,96 L 40,118 C 42,124 48,128 56,128 L 144,128 C 152,128 158,124 160,118 L 164,96 C 166,84 166,72 164,60 L 160,30 C 158,18 150,12 140,12 Z",
                    .stroke = StrokeStyle::solid(Color::opaqueSeparator(), 1.5f),
                },
                SvgPath{.d = "M 50,46 L 58,30 L 142,30 L 150,46 Z", .fill = FillStyle::solid(Color::elevatedBackground()), .stroke = StrokeStyle::solid(Color::separator(), 1.f)},
                SvgPath{.d = "M 50,108 L 58,124 L 142,124 L 150,108 Z", .fill = FillStyle::solid(Color::elevatedBackground()), .stroke = StrokeStyle::solid(Color::separator(), 1.f)},
                SvgRect{.x = 58.f, .y = 54.f, .width = 32.f, .height = 38.f, .cornerRadius = CornerRadius{6.f}, .fill = FillStyle::solid(Color::elevatedBackground()), .stroke = StrokeStyle::solid(Color::separator(), 1.f)},
                SvgRect{.x = 110.f, .y = 54.f, .width = 32.f, .height = 38.f, .cornerRadius = CornerRadius{6.f}, .fill = FillStyle::solid(Color::elevatedBackground()), .stroke = StrokeStyle::solid(Color::separator(), 1.f)},
                SvgConditional{.when = flowFace, .children = {
                    SvgCircle{.cx = 100.f, .cy = 50.f, .r = faceRadius, .fill = faceFill},
                    SvgCircle{.cx = 100.f, .cy = 50.f, .r = 4.f, .fill = FillStyle::solid(Color::accent())},
                }},
                SvgConditional{.when = flowFeet, .children = {
                    SvgCircle{.cx = 74.f, .cy = 100.f, .r = 3.f, .fill = FillStyle::solid(Color::accent())},
                    SvgCircle{.cx = 126.f, .cy = 100.f, .r = 3.f, .fill = FillStyle::solid(Color::accent())},
                }},
                SvgConditional{.when = flowWind, .children = {
                    SvgPath{.d = "M 60,38 L 64,32", .stroke = StrokeStyle::solid(Color::accent(), 1.5f)},
                    SvgPath{.d = "M 100,36 L 100,28", .stroke = StrokeStyle::solid(Color::accent(), 1.5f)},
                    SvgPath{.d = "M 140,38 L 136,32", .stroke = StrokeStyle::solid(Color::accent(), 1.5f)},
                }},
            }},
        };
    }
};

inline Element makeCarTopdownElement(Reactive::Bindable<bool> face, Reactive::Bindable<bool> feet, Reactive::Bindable<bool> wind) {
    return CarTopdown{.flowFace = face, .flowFeet = feet, .flowWind = wind};
}

} // namespace car
