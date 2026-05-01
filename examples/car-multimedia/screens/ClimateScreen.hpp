#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"
#include "../components/CarTopdown.hpp"
#include "../components/Chip.hpp"
#include "../components/FanBarsLarge.hpp"
#include "ZonePanel.hpp"

namespace car {

struct ClimateScreen : ViewModifiers<ClimateScreen> {
    Reactive::Signal<State> state;
    std::function<void(ClimateState)> onChange;
    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        auto patch = [state = state, onChange = onChange](auto&& fn) {
            ClimateState next = state().climate;
            fn(next);
            if (onChange) onChange(next);
        };
        Element centerPanel = makeCard(
            VStack {
                .spacing = 0.f,
                .alignment = Alignment::Stretch,
                .children = children(
                    Text {.text = "AIR DISTRIBUTION", .font = Font::caption2(), .color = Color::tertiary()},
                    makeCarTopdownElement(
                        Reactive::Bindable<bool> {[s = state] { return hasFlow(s().climate, FlowMode::Face); }},
                        Reactive::Bindable<bool> {[s = state] { return hasFlow(s().climate, FlowMode::Feet); }},
                        Reactive::Bindable<bool> {[s = state] { return hasFlow(s().climate, FlowMode::Window); }}
                    )
                        .padding(theme().space4, 0.f, 0.f, 0.f)
                        .flex(1.f, 1.f, 0.f),
                    Grid {
                        .columns = 3,
                        .horizontalSpacing = theme().space2,
                        .verticalSpacing = theme().space2,
                        .children = children(
                            Chip {.icon = IconName::Face, .label = "Face", .active = Reactive::Bindable<bool> {[s = state] { return hasFlow(s().climate, FlowMode::Face); }}, .onTap = [patch] { patch([](ClimateState &c) { auto it = std::find(c.flow.begin(), c.flow.end(), FlowMode::Face); if (it == c.flow.end()) c.flow.push_back(FlowMode::Face); else c.flow.erase(it); }); }},
                            Chip {.icon = IconName::DirectionsWalk, .label = "Feet", .active = Reactive::Bindable<bool> {[s = state] { return hasFlow(s().climate, FlowMode::Feet); }}, .onTap = [patch] { patch([](ClimateState &c) { auto it = std::find(c.flow.begin(), c.flow.end(), FlowMode::Feet); if (it == c.flow.end()) c.flow.push_back(FlowMode::Feet); else c.flow.erase(it); }); }},
                            Chip {.icon = IconName::Window, .label = "Window", .active = Reactive::Bindable<bool> {[s = state] { return hasFlow(s().climate, FlowMode::Window); }}, .onTap = [patch] { patch([](ClimateState &c) { auto it = std::find(c.flow.begin(), c.flow.end(), FlowMode::Window); if (it == c.flow.end()) c.flow.push_back(FlowMode::Window); else c.flow.erase(it); }); }}
                        ),
                    }
                        .padding(theme().space4, 0.f, 0.f, 0.f),
                    Divider {.orientation = Divider::Orientation::Horizontal}.padding(theme().space4, 0.f, theme().space4, 0.f),
                    Grid {
                        .columns = 2,
                        .horizontalSpacing = theme().space2,
                        .verticalSpacing = theme().space2,
                        .children = children(
                            Chip {.icon = IconName::AcUnit, .label = "A/C", .active = Reactive::Bindable<bool> {[s = state] { return s().climate.ac; }}, .onTap = [patch] { patch([](ClimateState &c) { c.ac = !c.ac; }); }},
                            Chip {.icon = IconName::Autorenew, .label = "Auto", .active = Reactive::Bindable<bool> {[s = state] { return s().climate.auto_; }}, .onTap = [patch] { patch([](ClimateState &c) { c.auto_ = !c.auto_; }); }},
                            Chip {.icon = IconName::Window, .label = "Defrost", .active = Reactive::Bindable<bool> {[s = state] { return s().climate.defrost; }}, .onTap = [patch] { patch([](ClimateState &c) { c.defrost = !c.defrost; }); }},
                            Chip {.icon = IconName::Recycling, .label = "Recirc", .active = Reactive::Bindable<bool> {[s = state] { return s().climate.recirc; }}, .onTap = [patch] { patch([](ClimateState &c) { c.recirc = !c.recirc; }); }}
                        ),
                    },
                    Divider {.orientation = Divider::Orientation::Horizontal}.padding(theme().space4, 0.f, theme().space4, 0.f),
                    Text {.text = "FAN SPEED", .font = Font::caption2(), .color = Color::tertiary()},
                    FanBarsLarge {.value = Reactive::Bindable<int> {[s = state] { return s().climate.fan; }}, .onChange = [patch](int v) { patch([v](ClimateState &c) { c.fan = v; }); }}
                ),
            },
            24.f
        );
        return Grid {
            .columns = 3,
            .horizontalSpacing = theme().space4,
            .verticalSpacing = theme().space4,
            .horizontalAlignment = Alignment::Stretch,
            .verticalAlignment = Alignment::Stretch,
            .children = children(
                ZonePanel{.label = "DRIVER", .temp = [s = state] { return s().climate.tempL; },
                          .onChange = [patch](float v) { patch([v](ClimateState& c) { c.tempL = std::clamp(v, 16.f, 28.f); }); }},
                std::move(centerPanel),
                ZonePanel{.label = "PASSENGER", .temp = [s = state] { return s().climate.tempR; },
                          .onChange = [patch](float v) { patch([v](ClimateState& c) { c.tempR = std::clamp(v, 16.f, 28.f); }); }}
            ),
        }.padding(theme().space4);
    }
};

} // namespace car
