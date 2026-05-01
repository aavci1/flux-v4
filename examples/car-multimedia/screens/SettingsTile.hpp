#pragma once

#include "../Common.hpp"
#include "../components/Chevron.hpp"

namespace car {

struct SettingsTile : ViewModifiers<SettingsTile> {
    IconName icon = IconName::Settings;
    std::string title;
    std::string detail;
    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        return HStack {
            .spacing = theme().space4,
            .alignment = Alignment::Center,
            .children = children(
                ZStack{.horizontalAlignment = Alignment::Center, .verticalAlignment = Alignment::Center,
                       .children = children(Rectangle{}.size(40.f, 40.f).fill(Color::selectedContentBackground()).cornerRadius(theme().radiusMedium),
                                            Icon{.name = icon, .size = 20.f, .color = Color::accent()})},
                VStack{.spacing = theme().space1, .alignment = Alignment::Start,
                       .children = children(Text{.text = title, .font = Font::headline(), .color = Color::primary()},
                                            Text{.text = detail, .font = Font::caption(), .color = Color::secondary()})}
                    .flex(1.f, 1.f, 0.f),
                Chevron{}
            ),
        }.padding(18.f, 18.f, 18.f, 18.f)
            .fill(Color::controlBackground()).stroke(Color::separator(), 1.f).cornerRadius(theme().radiusMedium).cursor(Cursor::Hand);
    }
};

} // namespace car
