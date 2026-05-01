#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"
#include "../components/Bullet.hpp"
#include "../components/CardHeader.hpp"
#include "../components/ManeuverBadge.hpp"
#include "../components/StylizedMap.hpp"

namespace car {

struct MapPreviewCard : ViewModifiers<MapPreviewCard> {
    Reactive::Signal<State> state;
    std::function<void(Screen)> onNavigate;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Element destination = VStack {
            .spacing = theme().space1,
            .alignment = Alignment::Start,
            .children = children(
                Text{.text = Reactive::Bindable<std::string>{[s = state] { return s().nav.destination; }},
                     .font = Font{.size = 28.f, .weight = 400.f}, .color = Color::primary()},
                HStack {
                    .spacing = theme().space2,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text{.text = Reactive::Bindable<std::string>{[s = state] { return s().nav.eta; }}, .font = Font::subheadline(), .color = Color::secondary()},
                        Bullet{},
                        Text{.text = Reactive::Bindable<std::string>{[s = state] { return s().nav.distance; }}, .font = Font::subheadline(), .color = Color::secondary()},
                        Bullet{},
                        Text{.text = Reactive::Bindable<std::string>{[s = state] { return "via " + s().nav.via; }}, .font = Font::subheadline(), .color = Color::secondary()}
                    ),
                }
            ),
        }.padding(0.f, 0.f, 14.f, 0.f);
        Element maneuverChip = glassCard(
            HStack {
                .spacing = theme().space3,
                .alignment = Alignment::Center,
                .children = children(
                    ManeuverBadge{.icon = IconName::TurnSlightRight, .sizeP = 36.f},
                    VStack {
                        .spacing = theme().space1,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text{.text = "In 1.4 km", .font = Font{.size = 20.f, .weight = 500.f}, .color = Color::primary()},
                            Text{.text = "Bear right onto A8", .font = Font::footnote(), .color = Color::secondary()}
                        ),
                    }
                ),
            },
            theme().space2);
        Element body = VStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                CardHeader{.icon = IconName::Navigation, .title = "NAVIGATION", .trailingChevron = true},
                destination,
                ZStack {
                    .horizontalAlignment = Alignment::Start,
                    .verticalAlignment = Alignment::Start,
                    .children = children(
                        makeStylizedMapElement().stroke(Color::separator(), 1.f).cornerRadius(theme().radiusLarge).clipContent(true),
                        std::move(maneuverChip).padding(14.f, 0.f, 0.f, 14.f)
                    ),
                }.flex(1.f, 1.f, 0.f)
            ),
        };
        return makeCard(body, 20.f)
            .cursor(Cursor::Hand)
            .onTap([onNavigate = onNavigate] { if (onNavigate) onNavigate(Screen::Map); });
    }
};

} // namespace car
