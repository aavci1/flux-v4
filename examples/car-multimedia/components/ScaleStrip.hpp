#pragma once

#include "../Common.hpp"

namespace car {

struct ScaleStrip : ViewModifiers<ScaleStrip> {
    Reactive::Bindable<float> pct{0.5f};
    float referenceHeight = 220.f;

    auto body() const {
        return Render{
            .measureFn = [referenceHeight = referenceHeight](LayoutConstraints const& constraints, LayoutHints const&) {
                float const height = std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f
                                         ? constraints.maxHeight
                                         : referenceHeight;
                return Size{std::max(44.f, constraints.minWidth), std::max(height, constraints.minHeight)};
            },
            .draw = [pct = pct](Canvas& canvas, Rect frame) {
                float const stripWidth = 4.f;
                float const stripX = frame.x + std::max(0.f, (frame.width - stripWidth) * 0.5f);
                canvas.drawRect(Rect{stripX, frame.y, stripWidth, frame.height},
                                CornerRadius{2.f},
                                FillStyle::linearGradient({
                                    GradientStop{0.00f, Color::hex(0xFF453A)},
                                    GradientStop{0.35f, Color::hex(0xFF9F0A)},
                                    GradientStop{0.50f, Color::hex(0x28CD41)},
                                    GradientStop{1.00f, Color::hex(0x0A84FF)},
                                }, Point{0.f, 0.f}, Point{0.f, 1.f}),
                                StrokeStyle::none());
                float const knobSize = std::min(28.f, std::max(16.f, frame.width * 0.65f));
                float const knobX = frame.x + std::max(0.f, (frame.width - knobSize) * 0.5f);
                float const travel = std::max(0.f, frame.height - knobSize);
                float const knobY = frame.y + (1.f - std::clamp(pct.evaluate(), 0.f, 1.f)) * travel;
                canvas.drawRect(Rect{knobX, knobY, knobSize, knobSize},
                                CornerRadius{knobSize * 0.5f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::accent(), 3.f));
            },
        };
    }
};

} // namespace car
