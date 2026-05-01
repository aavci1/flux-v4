#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"
#include "../components/AlbumArt.hpp"

namespace car {

struct QueueRow : ViewModifiers<QueueRow> {
    int index = 0;
    std::string title;
    std::string artist;
    std::string duration;
    Reactive::Bindable<bool> active{false};

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Reactive::Bindable<Color> titleColor{[active = active] {
            return active.evaluate() ? Color::accent() : Color::primary();
        }};
        Reactive::Bindable<FillStyle> rowFill{[active = active] {
            return FillStyle::solid(active.evaluate() ? Color::selectedContentBackground() : transparent());
        }};
        return HStack {
            .spacing = theme().space3,
            .alignment = Alignment::Center,
            .children = children(
                ZStack{.horizontalAlignment = Alignment::Center, .verticalAlignment = Alignment::Center,
                       .children = children(
                           Show(
                               [active = active] { return active.evaluate(); },
                               [] { return Icon{.name = IconName::GraphicEq, .size = 14.f, .weight = 600.f, .color = Color::accent()}; },
                               [index = index] {
                                   return Text{.text = std::to_string(index + 1), .font = Font::caption2(), .color = Color::tertiary(), .horizontalAlignment = HorizontalAlignment::Center};
                               }
                           )
                       )},
                AlbumArt{.seed = title, .sizeP = 36.f},
                VStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Start,
                    .children = children(
                        Text{.text = title, .font = Font{.size = 13.f, .weight = 500.f}, .color = titleColor, .wrapping = TextWrapping::NoWrap, .maxLines = 1},
                        Text{.text = artist, .font = Font::caption(), .color = Color::secondary()}
                    ),
                }.flex(1.f, 1.f, 0.f),
                Text{.text = duration, .font = Font::caption(), .color = Color::tertiary()}
            ),
        }.padding(theme().space3, theme().space5, theme().space3, theme().space5)
            .fill(rowFill);
    }
};

} // namespace car
