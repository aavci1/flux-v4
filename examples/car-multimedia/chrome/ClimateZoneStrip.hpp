#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct ClimateZoneStrip : ViewModifiers<ClimateZoneStrip> {
    enum class Side : std::uint8_t { Left, Right };
    std::string label;
    Reactive::Bindable<float> temp{21.5f};
    Side side = Side::Left;
    std::function<void(float)> onChange;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Element textBlock = VStack {
            .spacing = theme().space1,
            .alignment = side == Side::Left ? Alignment::Start : Alignment::End,
            .children = children(
                Text{.text = label, .font = Font::caption2(), .color = Color::tertiary()},
                Text{.text = Reactive::Bindable<std::string>{[temp = temp] { return format1(temp.evaluate()) + "°"; }},
                     .font = Font{.size = 22.f, .weight = 400.f}, .color = Color::primary()}
            ),
        };
        Element controls = HStack {
            .spacing = theme().space2,
            .alignment = Alignment::Center,
            .children = children(
                IconButton{.icon = IconName::Remove, .style = IconButton::Style{.size = 18.f},
                           .onTap = [onChange = onChange, temp = temp] { if (onChange) onChange(temp.evaluate() - 0.5f); }},
                IconButton{.icon = IconName::Add, .style = IconButton::Style{.size = 18.f},
                           .onTap = [onChange = onChange, temp = temp] { if (onChange) onChange(temp.evaluate() + 0.5f); }}
            ),
        };
        std::vector<Element> rowKids;
        if (side == Side::Left) {
            rowKids.push_back(textBlock);
            rowKids.push_back(Spacer{}.flex(1.f, 1.f));
            rowKids.push_back(controls);
        } else {
            rowKids.push_back(controls);
            rowKids.push_back(Spacer{}.flex(1.f, 1.f));
            rowKids.push_back(textBlock);
        }
        return HStack{.spacing = theme().space3, .alignment = Alignment::Center, .children = std::move(rowKids)}
            .padding(0.f, side == Side::Left ? 18.f : 28.f, 0.f, side == Side::Left ? 28.f : 18.f);
    }
};

} // namespace car
