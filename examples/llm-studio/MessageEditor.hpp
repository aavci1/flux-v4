#pragma once

#include <Flux.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

using namespace flux;

struct MessageEditor : ViewModifiers<MessageEditor> {
    std::string modelName;
    std::function<void(const std::string&)> onSend;
    /// When true, send is ignored and the send control is dimmed (e.g. while a reply is streaming).
    bool disabled = false;

    auto body() const {
        auto value = useState<std::string>("");

        Theme const& theme = useEnvironment<Theme>();

        return VStack {
            .spacing = 16.f,
            .alignment = Alignment::Start,
            .children = children(
                TextInput {
                    .value = value,
                    .placeholder = "Type your message here...",
                    .style = TextInput::Style::plain(),
                    .multiline = true,
                },
                HStack {
                    .spacing = 16.f,
                    .alignment = Alignment::Center,
                    .children = children(
                        Icon {
                            .name = IconName::Attachment,
                            .size = 20.f,
                            .weight = 300.f,
                            .color = theme.colorTextSecondary,
                        }.cursor(Cursor::Hand),
                        Spacer {},
                        Icon {
                            .name = IconName::ArrowUpward,
                            .size = 16.f,
                            .weight = 300.f,
                            .color = disabled ? Color::hex(0xC5C5C5) : theme.colorTextSecondary,
                        }
                        .cursor(disabled ? Cursor::Arrow : Cursor::Hand)
                        .stroke(StrokeStyle::solid(theme.colorTextSecondary, 2.f))
                        .padding(4.f)
                        .cornerRadius(12.f)
                        .onTap([value, onSend = onSend, disabled = disabled]() {
                            auto v = *value;

                            if (disabled || !onSend) {
                                return;
                            }

                            onSend(v);
                            value = "";
                        })
                    )
                }
            )
        }
        .fill(FillStyle::solid(Color::hex(0xF5F5F5)))
        .cornerRadius(8.f)
        .padding(16.f);
    }
};
