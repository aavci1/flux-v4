#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct CarSideView : ViewModifiers<CarSideView> {
    auto body() const {
        return Render{
            .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
                Size size{360.f, 110.f};
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
            .draw = [](Canvas& canvas, Rect frame) {
                constexpr float baseW = 360.f;
                constexpr float baseH = 110.f;
                auto wheel = [&canvas](float cx, float r, Color fill, Color stroke, float sw) {
                    canvas.drawRect(Rect{cx - r, 94.f - r, r * 2.f, r * 2.f}, CornerRadius{r},
                                    FillStyle::solid(fill), StrokeStyle::solid(stroke, sw));
                };
                canvas.save();
                canvas.translate(frame.x, frame.y);
                canvas.scale(frame.width / baseW, frame.height / baseH);
                canvas.drawRect(Rect{28.f, 58.f, 318.f, 48.f}, CornerRadius{18.f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{92.f, 26.f, 190.f, 48.f}, CornerRadius{20.f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{110.f, 34.f, 78.f, 32.f}, CornerRadius{6.f},
                                FillStyle::solid(Color::controlBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{196.f, 34.f, 84.f, 32.f}, CornerRadius{6.f},
                                FillStyle::solid(Color::controlBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                wheel(106.f, 18.f, Color::controlBackground(), Color::opaqueSeparator(), 1.5f);
                wheel(106.f, 9.f, Color::elevatedBackground(), Color::elevatedBackground(), 1.f);
                wheel(272.f, 18.f, Color::controlBackground(), Color::opaqueSeparator(), 1.5f);
                wheel(272.f, 9.f, Color::elevatedBackground(), Color::elevatedBackground(), 1.f);
                canvas.drawRect(Rect{44.f, 58.f, 32.f, 32.f}, CornerRadius{16.f},
                                FillStyle::solid(alphaBlue(0.18f)), StrokeStyle::none());
                canvas.drawRect(Rect{56.f, 70.f, 8.f, 8.f}, CornerRadius{4.f},
                                FillStyle::solid(Color::accent()), StrokeStyle::none());
                canvas.restore();
            },
            .pure = true,
        };
    }
};

inline Element makeCarSideViewElement() { return CarSideView{}; }

} // namespace car
