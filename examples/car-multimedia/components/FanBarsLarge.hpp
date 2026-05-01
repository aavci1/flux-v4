#pragma once

#include "../Common.hpp"

namespace car {

struct FanBarsLarge : ViewModifiers<FanBarsLarge> {
    Reactive::Bindable<int> value{4};
    std::function<void(int)> onChange;

    auto body() const {
        std::vector<Element> bars;
        bars.reserve(8);
        for (int i = 0; i < 8; ++i) {
            Reactive::Bindable<Color> fill{[value = value, i] {
                return i < value.evaluate() ? Color::accent() : Color::separator();
            }};
            bars.push_back(Rectangle{}.height(12.f + static_cast<float>(i) * 4.f)
                               .fill(fill)
                               .cornerRadius(3.f)
                               .flex(1.f, 1.f, 0.f)
                               .cursor(Cursor::Hand)
                               .onTap([onChange = onChange, i] { if (onChange) onChange(i + 1); }));
        }
        return HStack{.spacing = 4.f, .alignment = Alignment::End, .children = std::move(bars)}.height(44.f);
    }
};

} // namespace car
