#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Constants.hpp"
#include "NavRailItem.hpp"

namespace car {

struct NavRail : ViewModifiers<NavRail> {
    Reactive::Signal<State> state;
    std::function<void(Screen)> onChange;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        struct Item { IconName icon; std::string label; Screen screen; };
        std::vector<Item> items = {
            {IconName::Apps, "Home", Screen::Home},
            {IconName::Map, "Navigation", Screen::Map},
            {IconName::GraphicEq, "Media", Screen::Music},
            {IconName::Call, "Phone", Screen::Phone},
            {IconName::ModeFan, "Climate", Screen::Climate},
            {IconName::DirectionsCar, "Vehicle", Screen::Vehicle},
        };
        std::vector<Element> kids;
        kids.reserve(items.size() + 2);
        for (auto const& it : items) {
            kids.push_back(NavRailItem{
                .icon = it.icon,
                .label = it.label,
                .active = [s = state, screen = it.screen] { return s().active == screen; },
                .onTap = [onChange = onChange, screen = it.screen] { if (onChange) onChange(screen); },
            });
        }
        kids.push_back(Spacer{}.flex(1.f, 1.f));
        kids.push_back(NavRailItem{
            .icon = IconName::Settings,
            .label = "Settings",
            .active = [s = state] { return s().active == Screen::Settings; },
            .onTap = [onChange = onChange] { if (onChange) onChange(Screen::Settings); },
        });
        return VStack {
            .spacing = theme().space1,
            .alignment = Alignment::Stretch,
            .children = std::move(kids),
        }
            .padding(14.f, theme().space3, 14.f, theme().space3)
            .fill(Color::elevatedBackground());
    }
};

} // namespace car
