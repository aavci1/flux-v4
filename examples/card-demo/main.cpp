#include <Flux.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/Card.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Show.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/Toggle.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>
#include <vector>

using namespace flux;

namespace {

struct ExpandableCard : ViewModifiers<ExpandableCard> {
    IconName icon = IconName::Dashboard;
    Color accent = Color::accent();
    std::string title;
    std::string summary;
    std::string detail;

    bool operator==(ExpandableCard const &) const = default;

    Element body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto expanded = useState(false);
        Color const accentColor = accent;

        std::vector<Element> content;
        content.push_back(
            HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Center,
                .children = children(
                    Icon {
                        .name = icon,
                        .size = 18.f,
                        .color = accent,
                    },
                    VStack {
                        .spacing = theme.space1,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text {
                                .text = title,
                                .font = Font::headline(),
                                .color = Color::primary(),
                            },
                            Text {
                                .text = summary,
                                .font = Font::footnote(),
                                .color = Color::secondary(),
                                .wrapping = TextWrapping::Wrap,
                            })
                    }
                        .flex(1.f, 1.f, 0.f),
                    Icon {
                        .name = [expanded] {
                            return expanded.get() ? IconName::ExpandLess : IconName::ExpandMore;
                        },
                        .size = 18.f,
                        .color = Color::tertiary(),
                    })
            });

        content.push_back(
            Show([expanded] {
                return expanded.get();
            }, [detailText = detail] {
                return Element {
                    Text {
                        .text = detailText,
                        .font = Font::body(),
                        .color = Color::secondary(),
                        .wrapping = TextWrapping::Wrap,
                    }};
            }));

        return Card {
            .child = VStack {
                .spacing = theme.space3,
                .alignment = Alignment::Stretch,
                .children = std::move(content),
            },
            .style = Card::Style {
                .padding = theme.space4,
                .cornerRadius = theme.radiusXLarge,
                .borderColor = [expanded, accentColor, theme] {
                    return expanded.get() ? accentColor : theme.separatorColor;
                },
                .shadow = [expanded, theme] {
                    return expanded.get()
                               ? ShadowStyle {
                                     .radius = theme.shadowRadiusPopover,
                                     .offset = {0.f, theme.shadowOffsetYPopover},
                                     .color = Color {0.f, 0.f, 0.f, 0.12f},
                                 }
                               : ShadowStyle::none();
                },
            },
        }
            .cursor(Cursor::Hand)
            .onTap([expanded] {
                expanded = !*expanded;
            });
    }
};

struct CardDemoView {
    Element body() const {
        Theme const &theme = useEnvironment<Theme>();

        auto accentBorder = useState(true);
        auto dropShadow = useState(true);

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = theme.space4,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        Text {
                            .text = "Card",
                            .font = Font::largeTitle(),
                            .color = Color::primary(),
                        },
                        Text {
                            .text =
                                "Reusable surface container for elevated content. This demo shows default styling, "
                                "custom borders, shadows, and composition with controls.",
                            .font = Font::body(),
                            .color = Color::secondary(),
                            .wrapping = TextWrapping::Wrap,
                        },
                        Card {
                            .child = HStack {
                                .spacing = theme.space4,
                                .alignment = Alignment::Center,
                                .children = children(
                                    VStack {
                                        .spacing = theme.space1,
                                        .alignment = Alignment::Start,
                                        .children = children(
                                            Text {
                                                .text = "Style Knobs",
                                                .font = Font::headline(),
                                                .color = Color::primary(),
                                            },
                                            Text {
                                                .text = "These toggles feed the next card's border and shadow.",
                                                .font = Font::footnote(),
                                                .color = Color::secondary(),
                                                .wrapping = TextWrapping::Wrap,
                                            })
                                    }
                                        .flex(1.f, 1.f, 0.f),
                                    VStack {
                                        .spacing = theme.space2,
                                        .alignment = Alignment::Start,
                                        .children = children(
                                            HStack {
                                                .spacing = theme.space2,
                                                .alignment = Alignment::Center,
                                                .children = children(
                                                    Toggle {.value = accentBorder},
                                                    Text {
                                                        .text = "Accent border",
                                                        .font = Font::footnote(),
                                                        .color = Color::primary(),
                                                    })
                                            },
                                            HStack {
                                                .spacing = theme.space2,
                                                .alignment = Alignment::Center,
                                                .children = children(
                                                    Toggle {.value = dropShadow},
                                                    Text {
                                                        .text = "Drop shadow",
                                                        .font = Font::footnote(),
                                                        .color = Color::primary(),
                                                    })
                                            })
                                    })
                            },
                        },
                        Card {
                            .child = VStack {
                                .spacing = theme.space2,
                                .alignment = Alignment::Start,
                                .children = children(
                                    Text {
                                        .text = "Default card",
                                        .font = Font::headline(),
                                        .color = Color::primary(),
                                    },
                                    Text {
                                        .text = "Uses the framework defaults: elevated background, separator border, large radius, and standard padding.",
                                        .font = Font::body(),
                                        .color = Color::secondary(),
                                        .wrapping = TextWrapping::Wrap,
                                    })
                            },
                        },
                        Card {
                            .child = VStack {
                                .spacing = theme.space3,
                                .alignment = Alignment::Stretch,
                                .children = children(
                                    Text {
                                        .text = "Customized card",
                                        .font = Font::headline(),
                                        .color = Color::primary(),
                                    },
                                    Text {
                                        .text = "Same component, different tokens. This is the replacement path for the demo-specific surface wrappers.",
                                        .font = Font::body(),
                                        .color = Color::secondary(),
                                        .wrapping = TextWrapping::Wrap,
                                    },
                                    HStack {
                                        .spacing = theme.space2,
                                        .alignment = Alignment::Center,
                                        .children = children(
                                            Icon {
                                                .name = IconName::Palette,
                                                .size = 18.f,
                                                .color = Color::accent(),
                                            },
                                            Text {
                                                .text = [accentBorder] {
                                                    return accentBorder.get()
                                                               ? std::string {"Accent border enabled"}
                                                               : std::string {"Neutral border enabled"};
                                                },
                                                .font = Font::footnote(),
                                                .color = Color::secondary(),
                                            },
                                            Spacer {},
                                            Text {
                                                .text = [dropShadow] {
                                                    return dropShadow.get()
                                                               ? std::string {"Shadow on"}
                                                               : std::string {"Shadow off"};
                                                },
                                                .font = Font::footnote(),
                                                .color = Color::tertiary(),
                                            })
                                    })
                            },
                            .style = Card::Style {
                                .padding = theme.space4,
                                .cornerRadius = theme.radiusXLarge,
                                .borderColor = [accentBorder, theme] {
                                    return accentBorder.get() ? theme.accentColor : theme.separatorColor;
                                },
                                .shadow = [dropShadow, theme] {
                                    return dropShadow.get()
                                               ? ShadowStyle {
                                                     .radius = theme.shadowRadiusPopover,
                                                     .offset = {0.f, theme.shadowOffsetYPopover},
                                                     .color = Color {0.f, 0.f, 0.f, 0.12f},
                                                 }
                                               : ShadowStyle::none();
                                },
                            },
                        },
                        Card {
                            .child = VStack {
                                .spacing = theme.space3,
                                .alignment = Alignment::Stretch,
                                .children = children(
                                    Text {
                                        .text = "Composed with other controls",
                                        .font = Font::headline(),
                                        .color = Color::primary(),
                                    },
                                    Text {
                                        .text = "Cards are just containers. Put buttons, toggles, metrics, or custom layouts inside them.",
                                        .font = Font::body(),
                                        .color = Color::secondary(),
                                        .wrapping = TextWrapping::Wrap,
                                    },
                                    HStack {
                                        .spacing = theme.space2,
                                        .alignment = Alignment::Center,
                                        .children = children(
                                            Button {
                                                .label = "Primary",
                                                .onTap = [] {},
                                            },
                                            Button {
                                                .label = "Secondary",
                                                .variant = ButtonVariant::Secondary,
                                                .onTap = [] {},
                                            })
                                    })
                            },
                            .style = Card::Style {
                                .padding = theme.space4,
                                .cornerRadius = theme.radiusLarge,
                                .backgroundColor = theme.controlBackgroundColor,
                            },
                        },
                        ExpandableCard {
                            .icon = IconName::AutoAwesome,
                            .accent = theme.accentColor,
                            .title = "Interactive card",
                            .summary = "Hover or tap to emphasize the card surface.",
                            .detail = "The component does not own higher-level behavior. State, hover feedback, "
                                      "and expansion remain in user code while the framework handles the surface.",
                        },
                        ExpandableCard {
                            .icon = IconName::DashboardCustomize,
                            .accent = theme.successColor,
                            .title = "Another surface variant",
                            .summary = "Same primitive, different accent and content.",
                            .detail = "This is the intended replacement for demo-local section cards, stat cards, "
                                      "and other repeated panel wrappers that only differed by border or shadow.",
                        })
                }
                    .padding(theme.space6))
        };
    }
};

} // namespace

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    auto &w = app.createWindow<Window>({
        .size = {820, 860},
        .title = "Flux — Card demo",
        .resizable = true,
    });
    w.setView<CardDemoView>();
    return app.exec();
}
