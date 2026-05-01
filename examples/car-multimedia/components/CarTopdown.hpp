#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct CarTopdown : ViewModifiers<CarTopdown> {
    Reactive::Bindable<bool> flowFace{false};
    Reactive::Bindable<bool> flowFeet{false};
    Reactive::Bindable<bool> flowWind{false};

    auto body() const {
        return Render{
            .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
                Size size{200.f, 140.f};
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
            .draw = [flowFace = flowFace, flowFeet = flowFeet, flowWind = flowWind](Canvas& canvas, Rect frame) {
                constexpr float baseW = 200.f;
                constexpr float baseH = 140.f;
                auto ventStroke = [&canvas](float x1, float y1, float x2, float y2) {
                    float const w = std::max(2.f, std::abs(x2 - x1));
                    float const h = std::max(2.f, std::abs(y2 - y1));
                    canvas.drawRect(Rect{std::min(x1, x2), std::min(y1, y2), w, h},
                                    CornerRadius{1.f}, FillStyle::solid(Color::accent()),
                                    StrokeStyle::none());
                };
                canvas.save();
                canvas.translate(frame.x, frame.y);
                canvas.scale(frame.width / baseW, frame.height / baseH);
                canvas.drawRect(Rect{36.f, 12.f, 128.f, 116.f}, CornerRadius{42.f},
                                FillStyle::solid(Color::controlBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{54.f, 30.f, 92.f, 18.f}, CornerRadius{6.f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{54.f, 106.f, 92.f, 18.f}, CornerRadius{6.f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{58.f, 54.f, 32.f, 38.f}, CornerRadius{6.f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{110.f, 54.f, 32.f, 38.f}, CornerRadius{6.f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                if (flowFace.evaluate()) {
                    canvas.drawRect(Rect{78.f, 28.f, 44.f, 44.f}, CornerRadius{22.f},
                                    FillStyle::solid(alphaBlue(0.15f)), StrokeStyle::none());
                    canvas.drawRect(Rect{96.f, 46.f, 8.f, 8.f}, CornerRadius{4.f},
                                    FillStyle::solid(Color::accent()), StrokeStyle::none());
                }
                if (flowFeet.evaluate()) {
                    canvas.drawRect(Rect{71.f, 97.f, 6.f, 6.f}, CornerRadius{3.f},
                                    FillStyle::solid(Color::accent()), StrokeStyle::none());
                    canvas.drawRect(Rect{123.f, 97.f, 6.f, 6.f}, CornerRadius{3.f},
                                    FillStyle::solid(Color::accent()), StrokeStyle::none());
                }
                if (flowWind.evaluate()) {
                    ventStroke(60.f, 38.f, 64.f, 32.f);
                    ventStroke(100.f, 36.f, 100.f, 28.f);
                    ventStroke(140.f, 38.f, 136.f, 32.f);
                }
                canvas.restore();
            },
        };
    }
};

inline Element makeCarTopdownElement(Reactive::Bindable<bool> face, Reactive::Bindable<bool> feet, Reactive::Bindable<bool> wind) {
    return CarTopdown{.flowFace = face, .flowFeet = feet, .flowWind = wind};
}

} // namespace car
