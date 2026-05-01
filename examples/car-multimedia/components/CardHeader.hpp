#pragma once

#include "../Common.hpp"

namespace car {

struct CardHeader : ViewModifiers<CardHeader> {
    IconName icon = IconName::Info;
    std::string title;
    bool trailingChevron = false;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        std::vector<Element> kids;
        kids.push_back(Icon{.name = icon, .size = 16.f, .color = Color::tertiary()});
        kids.push_back(Text{.text = title, .font = Font{.size = 11.f, .weight = 600.f}, .color = Color::tertiary()});
        kids.push_back(Spacer{}.flex(1.f, 1.f));
        if (trailingChevron) {
            kids.push_back(Icon{.name = IconName::ArrowForwardIos, .size = 12.f, .color = Color::tertiary()});
        }
        return HStack {
            .spacing = theme().space2,
            .alignment = Alignment::Center,
            .children = std::move(kids),
        }.padding(0.f, 0.f, 14.f, 0.f);
    }
};

} // namespace car
