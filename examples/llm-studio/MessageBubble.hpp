#pragma once

#include <Flux.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/ForEach.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "LlamaEngine.hpp"

using namespace flux;
using namespace llm_studio;

/// Single chat row: user bubble, reasoning card, or assistant reply bubble.
struct MessageBubble : ViewModifiers<MessageBubble> {
    ChatMessage message;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        auto content = message.text;
        auto style = FillStyle::none();

        if (message.role == ChatMessage::Role::User) {
            style = FillStyle::solid(Color::hex(0xBBDEFB));
        }
        else  if (message.role == ChatMessage::Role::Assistant) {
            style = FillStyle::solid(Color::hex(0xE8F5E9));
        }
        else if (message.role == ChatMessage::Role::Reasoning) {
            style = FillStyle::solid(Color::hex(0xFFF3E0));
        }

        return Text {
            .text = message.text,
            .style = theme.typeBody,
            .color = theme.colorTextPrimary,
            .verticalAlignment = VerticalAlignment::Top,
            .wrapping = TextWrapping::Wrap,
        }.padding(16.f)
        .fill(style)
        .cornerRadius(8.f);
    }
};
