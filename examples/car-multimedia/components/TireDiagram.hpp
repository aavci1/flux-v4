#pragma once

#include "../Common.hpp"

namespace car {

struct TireDiagram : ViewModifiers<TireDiagram> {
    auto body() const {
        return Render{
            .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
                Size size{200.f, 240.f};
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
                constexpr float baseW = 200.f;
                constexpr float baseH = 240.f;
                canvas.save();
                canvas.translate(frame.x, frame.y);
                canvas.scale(frame.width / baseW, frame.height / baseH);
                canvas.drawRect(Rect{41.f, 24.f, 118.f, 192.f}, CornerRadius{34.f},
                                FillStyle::solid(Color::controlBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{50.f, 40.f, 100.f, 14.f}, CornerRadius{5.f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.drawRect(Rect{50.f, 186.f, 100.f, 14.f}, CornerRadius{5.f},
                                FillStyle::solid(Color::elevatedBackground()),
                                StrokeStyle::solid(Color::separator(), 1.f));
                canvas.restore();
            },
            .pure = true,
        };
    }
};

inline Element makeTireDiagramElement() { return TireDiagram{}; }

} // namespace car
