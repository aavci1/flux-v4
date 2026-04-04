#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

using namespace flux;

struct Divider: ViewModifiers<Divider> {
    auto body() const {
        Theme const& theme = useEnvironment<Theme>();
        return Rectangle {
        }
        .size(0.f, 1.f)
        .cornerRadius(1.f)
        .fill(FillStyle::solid(theme.colorBorder));
    }
};

struct MenuItem: ViewModifiers<MenuItem> {
    IconName icon;
    std::string label;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        auto isHovered = useHover();
        auto isPressed = usePress();

        return HStack {
            .spacing = 12.f,
            .alignment = Alignment::Center,
            .children = children(
                Icon {
                    .name = icon,
                    .size = theme.typeTitle.size,
                    .weight = 200.f,
                    .color = theme.colorTextPrimary,
                },
                Text {
                    .text = label,
                    .style = theme.typeTitle,
                    .color = theme.colorTextPrimary,
                    .verticalAlignment = VerticalAlignment::Center,
                    .wrapping = TextWrapping::NoWrap,
                },
                Spacer {},
                Icon {
                    .name = IconName::MoreHoriz,
                    .size = theme.typeTitle.size,
                    .weight = 300.f,
                    .color = isHovered ? theme.colorTextPrimary : Colors::transparent,
                }
            )
        }
        .fill(isHovered ? FillStyle::solid(Color::hex(0xEBEDEF)) : FillStyle::none())
        // .shadow(isHovered ? ShadowStyle {
        //     .radius = 1.f,
        //     .offset = {0.f, 0.f},
        //     .color = Color::hex(0xC0C0C0)
        // } : ShadowStyle::none())
        // .cornerRadius(8.f)
        .cursor(Cursor::Hand)
        .padding(8.f);
    }
};

struct Menu: ViewModifiers<Menu> {
    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        return VStack {
            .spacing = 0.f,
            .children = children(
                MenuItem {
                    .icon = IconName::Home,
                    .label = "Home"
                },
                MenuItem {
                    .icon = IconName::Subtitles,
                    .label = "Chats"
                },
                MenuItem {
                    .icon = IconName::Robot,
                    .label = "Models"
                },
                MenuItem {
                    .icon = IconName::Settings,
                    .label = "Settings"
                }
            )
        };
    }
};

struct Sidebar : ViewModifiers<Sidebar> {
    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        return VStack {
            .spacing = 8.f,
            .alignment = Alignment::Stretch,
            .children = children(
                Text {
                    .text = "LLM Studio",
                    .style = theme.typeTitle,
                    .color = theme.colorTextPrimary,
                    .verticalAlignment = VerticalAlignment::Center,
                    .wrapping = TextWrapping::NoWrap,
                }.padding(16.f, 8.f, 8.f, 8.f),
                Divider {},
                Menu {}
            )
        }.width(240.f);
    }
};

struct MessageBox : ViewModifiers<MessageBox> {
    std::function<void(const std::string&)> onSend;

    auto body() const {
        auto value = useState<std::string>("");

        Theme const& theme = useEnvironment<Theme>();

        return VStack {
            .spacing = 16.f,
            .alignment = Alignment::Start,
            .children = children(
                TextArea {
                    .value = value,
                    .placeholder = "Type your message here...",
                    .style = TextArea::Style::plain()
                },
                HStack {
                    .spacing = 16.f,
                    .alignment = Alignment::Center,
                    .children = children(
                        Icon {
                            .name = IconName::Attachment,
                            .size = theme.typeBody.size,
                            .weight = 300.f,
                            .color = theme.colorTextSecondary,
                        }.cursor(Cursor::Hand),
                        Icon {
                            .name = IconName::Settings,
                            .size = theme.typeBody.size,
                            .weight = 300.f,
                            .color = theme.colorTextSecondary,
                        }.cursor(Cursor::Hand),
                        Spacer {},
                        Icon {
                            .name = IconName::Image,
                            .size = theme.typeBody.size,
                            .weight = 300.f,
                            .color = theme.colorTextSecondary,
                        }.cursor(Cursor::Hand),
                        Icon {
                            .name = IconName::Send,
                            .size = theme.typeBody.size,
                            .weight = 300.f,
                            .color = theme.colorTextSecondary,
                        }.cursor(Cursor::Hand)
                        .onTap([value = value, onSend = onSend]() {
                            if (onSend) {
                                onSend(*value);
                                value = "";
                            }
                        })
                    )
                }
            )
        }
        .fill(FillStyle::solid(Color::hex(0xEBEDEF)))
        .cornerRadius(8.f)
        // .shadow(ShadowStyle {
        //     .radius = 2.f,
        //     .offset = {0.f, 1.f},
        //     .color = Color::hex(0xC0C0C0)
        // })
        .padding(16.f);
    }
};

struct Content : ViewModifiers<Content> {
    auto body() const {
        auto messages = useState<std::vector<std::string>>({
            "Welcome to LLM Studio! This is a sample message to demonstrate the content area. You can type your messages in the input box below and see how the content area scrolls to accommodate new messages. Feel free to experiment with different message lengths to see how the layout adapts."
        });

        auto& theme = useEnvironment<Theme>();

        auto messagesValue = *messages;
        std::vector<Element> messageElements;
        for (const auto& message : messagesValue) {
            messageElements.push_back(
                Text {
                    .text = message,
                    .style = theme.typeBody,
                    .color = theme.colorTextPrimary,
                    .verticalAlignment = VerticalAlignment::Top,
                    .wrapping = TextWrapping::Wrap,
                }
            );
        }

        return VStack {
            .spacing = 8.f,
            .children = children(
                HStack {
                    .spacing = 8.f,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = "Content",
                            .style = theme.typeTitle,
                            .color = theme.colorTextPrimary,
                        }.padding(4.f, 8.f, 4.f, 8.f),
                        Spacer {},
                        Icon {
                            .name = IconName::MoreHoriz,
                            .size = theme.typeTitle.size + 4.f,
                            .weight = 300.f,
                            .color = theme.colorTextPrimary,
                        }.padding(4.f, 8.f, 4.f, 8.f)
                        .cursor(Cursor::Hand)
                    )
                },
                Divider {},
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = 16.f,
                            .children = messageElements
                        }
                    )
                }.flex(1.f),
                MessageBox {
                    .onSend = [messages](const std::string& message) {
                        auto m = *messages;
                        m.push_back(message);
                        messages = std::move(m);
                    }
                }
            )
        }
        .fill(FillStyle::solid(Color::hex(0xFFFFFF)))
        .cornerRadius(8.f)
        .shadow(ShadowStyle {
            .radius = 2.f,
            .offset = {0.f, 1.f},
            .color = Color::hex(0xC0C0C0)
        })
        .padding(16.f);
    }
};

struct Properties : ViewModifiers<Properties> {
    auto body() const {
        Theme const& theme = useEnvironment<Theme>();
        return VStack {
            .spacing = 8.f,
            .children = children(
                Text {
                    .text = "Properties",
                    .style = theme.typeTitle,
                    .color = theme.colorTextPrimary
                }.padding(16.f, 8.f, 8.f, 8.f),
                Divider {}
            )
        }.size(320.f, 0.f);
    }
};

struct AppRoot : ViewModifiers<AppRoot> {
    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        return HStack {
            .spacing = 16.f,
            .alignment = Alignment::Stretch,
            .children = children(
                Sidebar{},
                Content{}.flex(1.f, 1.f, 400.f),
                Properties{}
            ),
        }.padding(16.f);
    }
};

int main(int argc, char* argv[]) {
    Application app(argc, argv);

    auto& w = app.createWindow<Window>({
        .size = {1280, 800},
        .title = "LLM Studio",
    });

    w.setView(AppRoot {});

    return app.exec();
}
