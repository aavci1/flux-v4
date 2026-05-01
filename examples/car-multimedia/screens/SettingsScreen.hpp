#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"
#include "SettingsTile.hpp"

namespace car {

struct SettingsScreen : ViewModifiers<SettingsScreen> {
    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        std::vector<Element> tiles;
        struct Item {
            IconName icon;
            std::string title;
            std::string detail;
        };
        std::vector<Item> items = {
            {IconName::Palette, "Display", "Theme, brightness, day/night"},
            {IconName::GraphicEq, "Sound", "Audio, EQ, navigation prompts"},
            {IconName::Lock, "Security", "PIN, profiles, valet mode"},
            {IconName::Wifi, "Connectivity", "Bluetooth, Wi-Fi, CarPlay"},
            {IconName::DirectionsCar, "Vehicle", "Drive modes, regen, lighting"},
            {IconName::Settings, "Software", "v 4.2.1 / Up to date"},
        };
        for (auto const &item : items) {
            tiles.push_back(SettingsTile {.icon = item.icon, .title = item.title, .detail = item.detail});
        }
        return makeCard(
                   VStack {
                       .spacing = theme().space5,
                       .alignment = Alignment::Stretch,
                       .children = children(
                           Text {.text = "Settings", .font = Font::title2(), .color = Color::primary()},
                           Grid {.columns = 2, .horizontalSpacing = theme().space5, .verticalSpacing = theme().space5, .children = std::move(tiles)}.flex(1.f, 1.f, 0.f)
                       ),
                   },
                   32.f
        )
            .padding(theme().space6, theme().space6, theme().space6, theme().space6)
            .flex(1.f, 1.f, 0.f);
    }
};

} // namespace car
