#pragma once

#include "Common.hpp"
#include "AppState.hpp"

namespace car {

template<typename Fn>
void mutate(Reactive::Signal<State> const& state, Fn&& fn) {
    State next = state();
    fn(next);
    state = std::move(next);
}

inline std::string format1(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", v);
    return std::string(buf);
}

inline Color alphaBlue(float a) {
    return Color{0.04f, 0.52f, 1.f, a};
}

inline Color transparent() {
    return Color{0.f, 0.f, 0.f, 0.f};
}

inline bool hasFlow(ClimateState const& c, FlowMode mode) {
    return std::find(c.flow.begin(), c.flow.end(), mode) != c.flow.end();
}

inline Element glassCard(Element child, float padding) {
    return Card {
        .child = std::move(child),
        .style = Card::Style{
            .padding = padding,
            .cornerRadius = 14.f,
            .backgroundColor = Color::elevatedBackground(),
            .borderColor = Color::separator(),
        },
    };
}

inline Element makeCard(Element child, float padding = 20.f) {
    return Card {
        .child = std::move(child),
        .style = Card::Style{
            .padding = padding,
            .cornerRadius = 10.f,
            .backgroundColor = Color::elevatedBackground(),
            .borderColor = Color::separator(),
        },
    };
}

} // namespace car
