#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"
#include "../components/ManeuverBadge.hpp"
#include "../components/StylizedMap.hpp"

namespace car {

struct MapScreen : ViewModifiers<MapScreen> {
    Reactive::Signal<State> state;
    auto body() const {
        auto theme = useEnvironment<ThemeKey>();

        Element banner = glassCard(
            HStack {
                .spacing = theme().space3,
                .alignment = Alignment::Center,
                .children = children(
                    ManeuverBadge{.icon = IconName::TurnSlightRight, .sizeP = 56.f},
                    VStack {
                        .spacing = theme().space1,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text{.text = "In 1.4 km", .font = Font{.size = 22.f, .weight = 500.f}, .color = Color::primary()},
                            Text{.text = "Bear right onto A8 toward München", .font = Font::body(), .color = Color::secondary()}
                        ),
                    }.flex(1.f, 1.f, 0.f),
                    VStack {
                        .spacing = theme().space1,
                        .alignment = Alignment::End,
                        .children = children(
                            Text{.text = Reactive::Bindable<std::string>{[s = state] { return s().nav.eta; }},
                                 .font = Font{.size = 22.f, .weight = 500.f}, .color = Color::primary()},
                            Text{.text = Reactive::Bindable<std::string>{[s = state] { return s().nav.distance + " / ETA"; }},
                                 .font = Font::caption(), .color = Color::tertiary()}
                        ),
                    }
                ),
            }, theme().space4);
        Element zoomCluster = VStack {
            .spacing = theme().space2,
            .alignment = Alignment::End,
            .children = children(
                IconButton{.icon = IconName::Add, .style = {.size = 40.f}},
                IconButton{.icon = IconName::Remove, .style = {.size = 40.f}},
                IconButton{.icon = IconName::Explore, .style = {.size = 40.f}},
                IconButton{.icon = IconName::Layers, .style = {.size = 40.f}}
            ),
        };
        Element destCard = glassCard(
            VStack {
                .spacing = theme().space2,
                .alignment = Alignment::Start,
                .children = children(
                    Text{.text = "DESTINATION", .font = Font::caption2(), .color = Color::tertiary()},
                    Text{.text = Reactive::Bindable<std::string>{[s = state] { return s().nav.destination; }},
                         .font = Font{.size = 20.f, .weight = 500.f}, .color = Color::primary()},
                    Text{.text = Reactive::Bindable<std::string>{[s = state] { return s().nav.address; }},
                         .font = Font::subheadline(), .color = Color::secondary()},
                    HStack {
                        .spacing = theme().space2,
                        .alignment = Alignment::Center,
                        .children = children(
                            Button{.label = std::string{"Continue"}, .variant = ButtonVariant::Primary},
                            Button{.label = std::string{"End"}, .variant = ButtonVariant::Secondary}
                        ),
                    }.padding(14.f, 0.f, 0.f, 0.f)
                ),
            },
            theme().space4);
        Element overlayGrid = Grid {
            .columns = 4,
            .horizontalSpacing = theme().space4,
            .verticalSpacing = theme().space4,
            .horizontalAlignment = Alignment::Stretch,
            .verticalAlignment = Alignment::Stretch,
            .children = children(
                std::move(banner).colSpan(4u),
                Spacer {}.colSpan(3u).rowSpan(2u),
                std::move(zoomCluster).rowSpan(2u),
                std::move(destCard).colSpan(2u),
                Spacer {}.colSpan(2u)
            ),
        }
            .padding(theme().space4)
            .flex(1.f, 1.f, 0.f);
        return ZStack {
            .horizontalAlignment = Alignment::Start,
            .verticalAlignment = Alignment::Start,
            .children = children(
                makeStylizedMapElement(3.f).flex(1.f, 1.f, 0.f),
                std::move(overlayGrid)
            ),
        }.clipContent(true);
    }
};

} // namespace car
