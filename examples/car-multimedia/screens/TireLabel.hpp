#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"

namespace car {

struct TireLabel : ViewModifiers<TireLabel> {
    std::string label;
    float psi = 2.4f;
    TireStatus status = TireStatus::Ok;
    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        bool warn = status == TireStatus::Warn;
        return VStack {
            .spacing = theme().space1,
            .alignment = Alignment::Start,
            .children = children(
                Text{.text = label, .font = Font::caption2(), .color = Color::tertiary()},
                HStack{.spacing = 2.f, .alignment = Alignment::End,
                       .children = children(Text{.text = format1(psi), .font = Font{.size = 16.f, .weight = 500.f}, .color = warn ? Color::warning() : Color::primary()},
                                            Text{.text = "psi", .font = Font::caption2(), .color = Color::tertiary()})}
            ),
        }.padding(theme().space2, theme().space3, theme().space2, theme().space3)
            .fill(Color::controlBackground()).stroke(warn ? Color::warning() : Color::separator(), 1.f).cornerRadius(theme().radiusMedium);
    }
};

} // namespace car
