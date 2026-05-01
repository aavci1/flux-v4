#pragma once

#include "../Common.hpp"

namespace car {

struct Chevron : ViewModifiers<Chevron> {
    auto body() const {
        return Icon{.name = IconName::ArrowForwardIos, .size = 12.f, .color = Color::tertiary()};
    }
};

} // namespace car
