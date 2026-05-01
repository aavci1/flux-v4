#pragma once

#include "../Common.hpp"

namespace car {

struct Bullet : ViewModifiers<Bullet> {
    auto body() const {
        return Text{.text = "•", .font = Font::body(), .color = Color::quaternary()};
    }
};

} // namespace car
