#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"
#include "../components/BatteryGauge.hpp"
#include "../components/CardHeader.hpp"
#include "../components/Stat.hpp"

namespace car {

struct VehicleStatusCard : ViewModifiers<VehicleStatusCard> {
    Reactive::Signal<State> state;
    std::function<void(Screen)> onNavigate;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Element body = VStack {
            .spacing = theme().space2,
            .alignment = Alignment::Stretch,
            .children = children(
                CardHeader{.icon = IconName::DirectionsCar, .title = "VEHICLE", .trailingChevron = true},
                HStack {
                    .spacing = theme().space2,
                    .alignment = Alignment::End,
                    .children = children(
                        Text{.text = Reactive::Bindable<std::string>{[s = state] { return std::to_string(s().vehicleStats.range); }},
                             .font = Font{.size = 32.f, .weight = 400.f}, .color = Color::primary()},
                        Text{.text = "km range", .font = Font::body(), .color = Color::tertiary()}
                    ),
                }.padding(0.f, 0.f, 4.f, 0.f),
                BatteryGauge{.level = Reactive::Bindable<int>{[s = state] { return s().vehicleStats.battery; }}},
                Spacer{}.flex(1.f, 1.f),
                Grid {
                    .columns = 2,
                    .horizontalSpacing = theme().space3,
                    .verticalSpacing = theme().space3,
                    .children = children(
                        Stat{.label = "TRIP", .value = Reactive::Bindable<std::string>{[s = state] { return format1(s().vehicleStats.trip) + " km"; }}},
                        Stat{.label = "EFFICIENCY", .value = Reactive::Bindable<std::string>{[s = state] { return format1(s().vehicleStats.efficiency) + " kWh/100"; }}}
                    ),
                }
            ),
        };
        return makeCard(body, 20.f)
            .cursor(Cursor::Hand)
            .onTap([onNavigate = onNavigate] { if (onNavigate) onNavigate(Screen::Vehicle); });
    }
};

} // namespace car
