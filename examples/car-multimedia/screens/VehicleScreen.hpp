#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"
#include "../components/BatteryGauge.hpp"
#include "../components/CardHeader.hpp"
#include "../components/CarSideView.hpp"
#include "../components/Stat.hpp"
#include "../components/TireDiagram.hpp"
#include "ModeButton.hpp"
#include "TireLabel.hpp"

namespace car {

struct VehicleScreen : ViewModifiers<VehicleScreen> {
    Reactive::Signal<State> state;
    std::function<void(VehicleControls)> onChange;

    enum class Corner : std::uint8_t { TL, TR, BL, BR };
    static Element cornerWrap(Element label, Corner corner) {
        bool left = corner == Corner::TL || corner == Corner::BL;
        bool top = corner == Corner::TL || corner == Corner::TR;
        Element v = top ? Element{VStack{.spacing = 0.f, .alignment = Alignment::Stretch, .children = children(label, Spacer{}.flex(1.f, 1.f))}}
                        : Element{VStack{.spacing = 0.f, .alignment = Alignment::Stretch, .children = children(Spacer{}.flex(1.f, 1.f), label)}};
        return left ? Element{HStack{.spacing = 0.f, .alignment = Alignment::Stretch, .children = children(v, Spacer{}.flex(1.f, 1.f))}}
                    : Element{HStack{.spacing = 0.f, .alignment = Alignment::Stretch, .children = children(Spacer{}.flex(1.f, 1.f), v)}};
    }

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        TireSet tires = state().tires;
        Element energyCard = makeCard(
            VStack {
                .spacing = 0.f,
                .alignment = Alignment::Stretch,
                .children = children(
                    CardHeader{.icon = IconName::ElectricCar, .title = "ENERGY"},
                    HStack {
                        .spacing = theme().space2,
                        .alignment = Alignment::End,
                        .children = children(
                            Text{.text = Reactive::Bindable<std::string>{[s = state] { return std::to_string(s().vehicleStats.battery); }},
                                 .font = Font{.size = 56.f, .weight = 300.f}, .color = Color::primary()},
                            Text{.text = "%", .font = Font{.size = 22.f, .weight = 300.f}, .color = Color::secondary()},
                            Spacer{}.flex(1.f, 1.f),
                            HStack{.spacing = theme().space1, .alignment = Alignment::Center,
                                   .children = children(Icon{.name = IconName::Bolt, .size = 14.f, .weight = 600.f, .color = Color::accent()},
                                                        Text{.text = "Charging / 7.2 kW / 38 min to full", .font = Font{.size = 12.f, .weight = 500.f}, .color = Color::accent()})}
                        ),
                    },
                    BatteryGauge{.level = Reactive::Bindable<int>{[s = state] { return s().vehicleStats.battery; }}}.padding(14.f, 0.f, 0.f, 0.f),
                    Spacer{}.flex(1.f, 1.f),
                    ZStack{.horizontalAlignment = Alignment::Center, .verticalAlignment = Alignment::Center, .children = children(makeCarSideViewElement())}
                        .padding(theme().space6, 0.f, theme().space6, 0.f)
                        .flex(1.f, 1.f, 0.f),
                    Divider{.orientation = Divider::Orientation::Horizontal},
                    Grid{.columns = 4, .horizontalSpacing = theme().space3, .verticalSpacing = 0.f,
                         .children = children(
                             Stat{.label = "RANGE", .value = Reactive::Bindable<std::string>{[s = state] { return std::to_string(s().vehicleStats.range) + " km"; }}},
                             Stat{.label = "TRIP", .value = Reactive::Bindable<std::string>{[s = state] { return format1(s().vehicleStats.trip) + " km"; }}},
                             Stat{.label = "EFFICIENCY", .value = Reactive::Bindable<std::string>{[s = state] { return format1(s().vehicleStats.efficiency) + " kWh/100"; }}},
                             Stat{.label = "ODOMETER", .value = std::string{"42 481 km"}}
                         )}.padding(16.f, 0.f, 0.f, 0.f)
                ),
            }, 32.f);
        Element tireCard = makeCard(
            VStack {
                .spacing = 0.f,
                .alignment = Alignment::Stretch,
                .children = children(
                    CardHeader{.icon = IconName::TireRepair, .title = "TIRE PRESSURE"},
                    ZStack {
                        .horizontalAlignment = Alignment::Center,
                        .verticalAlignment = Alignment::Center,
                        .children = children(
                            makeTireDiagramElement(),
                            cornerWrap(TireLabel{.label = "FL", .psi = tires.fl.psi, .status = tires.fl.status}, Corner::TL),
                            cornerWrap(TireLabel{.label = "FR", .psi = tires.fr.psi, .status = tires.fr.status}, Corner::TR),
                            cornerWrap(TireLabel{.label = "RL", .psi = tires.rl.psi, .status = tires.rl.status}, Corner::BL),
                            cornerWrap(TireLabel{.label = "RR", .psi = tires.rr.psi, .status = tires.rr.status}, Corner::BR)
                        ),
                    }.flex(1.f, 1.f, 0.f)
                ),
            }, 20.f);
        auto setMode = [onChange = onChange](std::string mode) {
            if (onChange) onChange(VehicleControls{.mode = std::move(mode)});
        };
        Element modeCard = makeCard(
            VStack {
                .spacing = 0.f,
                .alignment = Alignment::Stretch,
                .children = children(
                    CardHeader{.icon = IconName::Tune, .title = "DRIVE MODE"},
                    Grid{.columns = 2, .horizontalSpacing = theme().space2, .verticalSpacing = theme().space2,
                         .children = children(
                             ModeButton{.label = "Comfort", .icon = IconName::Weekend, .active = Reactive::Bindable<bool>{[s = state] { return s().vehicleControls.mode == "Comfort"; }}, .onTap = [setMode] { setMode("Comfort"); }},
                             ModeButton{.label = "Sport", .icon = IconName::Flag, .active = Reactive::Bindable<bool>{[s = state] { return s().vehicleControls.mode == "Sport"; }}, .onTap = [setMode] { setMode("Sport"); }},
                             ModeButton{.label = "Eco", .icon = IconName::Eco, .active = Reactive::Bindable<bool>{[s = state] { return s().vehicleControls.mode == "Eco"; }}, .onTap = [setMode] { setMode("Eco"); }},
                             ModeButton{.label = "Off-road", .icon = IconName::Road, .active = Reactive::Bindable<bool>{[s = state] { return s().vehicleControls.mode == "Off-road"; }}, .onTap = [setMode] { setMode("Off-road"); }}
                         )}.flex(1.f, 1.f, 0.f)
                ),
            }, 20.f);
        return Grid {
            .columns = 3,
            .horizontalSpacing = theme().space4,
            .verticalSpacing = theme().space4,
            .horizontalAlignment = Alignment::Stretch,
            .verticalAlignment = Alignment::Stretch,
            .children = children(
                std::move(energyCard).colSpan(2u).rowSpan(2u),
                std::move(tireCard),
                std::move(modeCard)
            ),
        }
            .padding(theme().space4);
    }
};

} // namespace car
