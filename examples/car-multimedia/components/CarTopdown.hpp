#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct CarTopdown : ViewModifiers<CarTopdown> {
    Reactive::Bindable<bool> flowFace{false};
    Reactive::Bindable<bool> flowFeet{false};
    Reactive::Bindable<bool> flowWind{false};

    auto body() const {
        auto pulse = useAnimated<float>(0.f);
        if (!pulse.isRunning() && std::abs(*pulse) < 0.001f) {
            pulse.play(1.f, AnimationOptions{
                .transition = Transition::ease(3.f),
                .repeat = AnimationOptions::kRepeatForever,
                .autoreverse = true,
            });
        }

        float const pulseValue = pulse();
        float const faceRadius = 14.f + pulseValue * 14.f;
        FillStyle const faceFill = FillStyle::solid(alphaBlue(0.15f));

        std::vector<SvgNode> nodes{
            svg::path("M 60,12 C 50,12 42,18 40,30 L 36,60 C 34,72 34,84 36,96 L 40,118 C 42,124 48,128 56,128 L 144,128 C 152,128 158,124 160,118 L 164,96 C 166,84 166,72 164,60 L 160,30 C 158,18 150,12 140,12 Z", FillStyle::none(), StrokeStyle::solid(Color::opaqueSeparator(), 1.5f)),
            svg::path("M 50,46 L 58,30 L 142,30 L 150,46 Z", FillStyle::solid(Color::elevatedBackground()), StrokeStyle::solid(Color::separator(), 1.f)),
            svg::path("M 50,108 L 58,124 L 142,124 L 150,108 Z", FillStyle::solid(Color::elevatedBackground()), StrokeStyle::solid(Color::separator(), 1.f)),
            svg::rect(58.f, 54.f, 32.f, 38.f, CornerRadius{6.f}, FillStyle::solid(Color::elevatedBackground()), StrokeStyle::solid(Color::separator(), 1.f)),
            svg::rect(110.f, 54.f, 32.f, 38.f, CornerRadius{6.f}, FillStyle::solid(Color::elevatedBackground()), StrokeStyle::solid(Color::separator(), 1.f)),
        };
        if (flowFace.evaluate()) {
            nodes.push_back(svg::circle(100.f, 50.f, faceRadius, faceFill));
            nodes.push_back(svg::circle(100.f, 50.f, 4.f, FillStyle::solid(Color::accent())));
        }
        if (flowFeet.evaluate()) {
            nodes.push_back(svg::circle(74.f, 100.f, 3.f, FillStyle::solid(Color::accent())));
            nodes.push_back(svg::circle(126.f, 100.f, 3.f, FillStyle::solid(Color::accent())));
        }
        if (flowWind.evaluate()) {
            nodes.push_back(svg::path("M 60,38 L 64,32", FillStyle::none(), StrokeStyle::solid(Color::accent(), 1.5f)));
            nodes.push_back(svg::path("M 100,36 L 100,28", FillStyle::none(), StrokeStyle::solid(Color::accent(), 1.5f)));
            nodes.push_back(svg::path("M 140,38 L 136,32", FillStyle::none(), StrokeStyle::solid(Color::accent(), 1.5f)));
        }

        return Svg{
            .viewBox = Rect{0.f, 0.f, 200.f, 140.f},
            .preserveAspectRatio = SvgPreserveAspectRatio::Stretch,
            .children = std::move(nodes),
        };
    }
};

inline Element makeCarTopdownElement(Reactive::Bindable<bool> face, Reactive::Bindable<bool> feet, Reactive::Bindable<bool> wind) {
    return CarTopdown{.flowFace = face, .flowFeet = feet, .flowWind = wind};
}

} // namespace car
