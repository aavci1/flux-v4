#pragma once

#include "../Common.hpp"

namespace car {

struct TireDiagram : ViewModifiers<TireDiagram> {
    auto body() const {
        return Svg{
            .viewBox = Rect{0.f, 0.f, 200.f, 240.f},
            .preserveAspectRatio = SvgPreserveAspectRatio::Stretch,
            .children = {
                svg::path("M 64,24 C 54,24 46,32 44,44 L 38,80 C 34,100 34,140 38,160 L 44,196 C 46,208 54,216 64,216 L 136,216 C 146,216 154,208 156,196 L 162,160 C 166,140 166,100 162,80 L 156,44 C 154,32 146,24 136,24 Z", FillStyle::solid(Color::elevatedBackground()), StrokeStyle::solid(Color::opaqueSeparator(), 1.5f)),
                svg::path("M 50,52 L 58,40 L 142,40 L 150,52 Z", FillStyle::solid(Color::controlBackground()), StrokeStyle::solid(Color::separator(), 1.f)),
                svg::path("M 50,188 L 58,200 L 142,200 L 150,188 Z", FillStyle::solid(Color::controlBackground()), StrokeStyle::solid(Color::separator(), 1.f)),
            },
        };
    }
};

inline Element makeTireDiagramElement() { return TireDiagram{}; }

} // namespace car
