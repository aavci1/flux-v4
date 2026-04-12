#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

using namespace flux;

namespace lambda {

struct ChatsView : ViewModifiers<ChatsView> {
    const std::string name = "Chats";
    const IconName icon = IconName::ChatBubble;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        return Text {
            .text = name,
            .font = theme.fontDisplay,
            .color = theme.colorTextPrimary,
            .horizontalAlignment = HorizontalAlignment::Center,
            .verticalAlignment = VerticalAlignment::Center,
        };
    }
};

} // namespace lambda
