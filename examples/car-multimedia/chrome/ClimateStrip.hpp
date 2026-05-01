#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Constants.hpp"
#include "../Helpers.hpp"
#include "../components/Chip.hpp"
#include "../components/FanControl.hpp"
#include "ClimateZoneStrip.hpp"

namespace car {

struct ClimateStrip : ViewModifiers<ClimateStrip> {
    Reactive::Signal<State> state;
    std::function<void(ClimateState)> onChange;
    std::function<void()> onOpen;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();

        auto setTemp = [state = state, onChange = onChange](bool left, float v) {
            ClimateState next = state().climate;

            if (left) {
                next.tempL = std::clamp(v, 16.f, 28.f);
            }
            else {
                next.tempR = std::clamp(v, 16.f, 28.f);
            }

            if (next == state().climate) {
                return;
            }

            if (onChange) {
                onChange(next);
            }
        };

        auto patch = [state = state, onChange = onChange](auto&& fn) {
            ClimateState next = state().climate;
            fn(next);
            if (onChange) onChange(next);
        };

        return HStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                ClimateZoneStrip {
                    .label = "DRIVER",
                    .temp = Reactive::Bindable<float> {[s = state] { return s().climate.tempL; }},
                    .side = ClimateZoneStrip::Side::Left,
                    .onChange = [setTemp](float v) { setTemp(true, v); },
                }
                    .flex(1.f, 1.f, 0.f),
                Divider {.orientation = Divider::Orientation::Vertical},
                HStack {
                    .spacing = theme().space2,
                    .alignment = Alignment::Center,
                    .justifyContent = JustifyContent::Center,
                    .children = children(
                        Chip {.icon = IconName::AcUnit, .label = "A/C", .active = Reactive::Bindable<bool> {[s = state] { return s().climate.ac; }}, .onTap = [patch] { patch([](ClimateState &c) { c.ac = !c.ac; }); }},
                        Chip {.icon = IconName::AirlineSeatReclineNormal, .label = "Heated", .active = Reactive::Bindable<bool> {[s = state] { return s().climate.heated; }}, .onTap = [patch] { patch([](ClimateState &c) { c.heated = !c.heated; }); }},
                        FanControl {.value = Reactive::Bindable<int> {[s = state] { return s().climate.fan; }}, .max = 6, .onChange = [patch](int v) { patch([v](ClimateState &c) { c.fan = std::clamp(v, 1, 6); }); }},
                        IconButton {.icon = IconName::Tune, .style = IconButton::Style {.size = 18.f}, .onTap = onOpen}
                    ),
                }
                    .padding(0.f, theme().space6, 0.f, theme().space6),
                Divider {.orientation = Divider::Orientation::Vertical},
                ClimateZoneStrip {
                    .label = "PASSENGER",
                    .temp = Reactive::Bindable<float> {[s = state] { return s().climate.tempR; }},
                    .side = ClimateZoneStrip::Side::Right,
                    .onChange = [setTemp](float v) { setTemp(false, v); },
                }
                    .flex(1.f, 1.f, 0.f)
            ),
        }
            .fill(Color::elevatedBackground());
    }
};

} // namespace car
