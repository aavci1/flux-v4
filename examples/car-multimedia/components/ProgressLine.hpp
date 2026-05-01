#pragma once

#include "../Common.hpp"

namespace car {

struct ProgressLine : ViewModifiers<ProgressLine> {
    Reactive::Bindable<float> progress{0.f};
    std::string startText = "1:24";
    std::string endText = "3:58";

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        return VStack {
            .spacing = 6.f,
            .alignment = Alignment::Stretch,
            .children = children(
                Render{
                    .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
                        float const width = std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f
                                                ? constraints.maxWidth
                                                : 0.f;
                        return Size{std::max(width, constraints.minWidth), 8.f};
                    },
                    .draw = [progress = progress](Canvas& canvas, Rect frame) {
                        float const pct = std::clamp(progress.evaluate(), 0.f, 1.f);
                        float const trackY = frame.y + std::max(0.f, (frame.height - 3.f) * 0.5f);
                        Rect const track{frame.x, trackY, std::max(0.f, frame.width), 3.f};
                        canvas.drawRect(track, CornerRadius{2.f}, FillStyle::solid(Color::separator()), StrokeStyle::none());
                        canvas.drawRect(Rect{frame.x, trackY, std::max(0.f, frame.width * pct), 3.f},
                                        CornerRadius{2.f}, FillStyle::solid(Color::accent()), StrokeStyle::none());
                    },
                },
                HStack {
                    .spacing = theme().space2,
                    .alignment = Alignment::Center,
                    .justifyContent = JustifyContent::SpaceBetween,
                    .children = children(
                        Text{.text = startText, .font = Font::caption2(), .color = Color::tertiary()},
                        Text{.text = endText, .font = Font::caption2(), .color = Color::tertiary()}
                    ),
                }
            ),
        };
    }
};

} // namespace car
