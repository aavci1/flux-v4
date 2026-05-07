#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Popover.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <functional>
#include <string>
#include <vector>

using namespace flux;

struct PopoverDemoRoot {
    auto body() const {
        auto showArrow = useState<bool>(true);
        auto dismissOutside = useState<bool>(true);
        auto [showPopover, hidePopover, popoverOpen] = usePopover();

        auto theme = useEnvironment<ThemeKey>();

        std::vector<Element> scrollChildren;

        auto addSection = [&](char const *heading) {
            scrollChildren.push_back(
                Text {
                    .text = heading,
                    .font = Font::title(),
                    .color = Color::primary(),
                }
                    .padding(8.f, 0.f, 8.f, 0.f)
            );
        };

        Theme const activeTheme = theme();
        auto regularActionButton = [activeTheme](std::string label,
                                                 ButtonVariant variant,
                                                 std::function<void()> action) -> Element {
            Color fill = activeTheme.accentColor;
            Color labelColor = activeTheme.accentForegroundColor;
            StrokeStyle stroke = StrokeStyle::none();
            if (variant == ButtonVariant::Secondary) {
                fill = activeTheme.elevatedBackgroundColor;
                labelColor = activeTheme.labelColor;
                stroke = StrokeStyle::solid(activeTheme.separatorColor, 1.f);
            }

            auto activate = [action] {
                if (action) {
                    action();
                }
            };
            auto handleKey = [activate](KeyCode key, Modifiers) {
                if (key == keys::Return || key == keys::Space) {
                    activate();
                }
            };

            return Text {
                .text = std::move(label),
                .font = activeTheme.headlineFont,
                .color = labelColor,
                .horizontalAlignment = HorizontalAlignment::Center,
                .verticalAlignment = VerticalAlignment::Center,
            }
                .fill(FillStyle::solid(fill))
                .stroke(stroke)
                .cornerRadius(CornerRadius {activeTheme.radiusLarge})
                .padding(activeTheme.space3, activeTheme.space4, activeTheme.space3, activeTheme.space4)
                .cursor(Cursor::Hand)
                .focusable(true)
                .onKeyDown(std::function<void(KeyCode, Modifiers)> {handleKey})
                .onTap(std::function<void()> {activate});
        };

        addSection("Placement");
        scrollChildren.push_back(
            Text {
                .text = "Scroll so triggers sit near window edges to see flip.",
                .font = Font::caption(),
                .color = Color::secondary(),
                .wrapping = TextWrapping::Wrap,
            }
                .padding(8.f)
                .flex(1.f)
        );

        auto addPlacementButton = [&](char const *label, PopoverPlacement placement) {
            scrollChildren.push_back(Button {
                .label = label,
                .variant = ButtonVariant::Secondary,
                .onTap = [=] {
                    showPopover(Popover {
                        .content = VStack {
                            .spacing = 8.f,
                            .alignment = Alignment::Start,
                            .children = children(
                                Text {.text = std::string(label), .font = Font::title3(), .color = Color::primary()},
                                Text {.text = "Placement follows preference when space allows.", .font = Font::footnote(), .color = Color::secondary(), .wrapping = TextWrapping::Wrap}
                                    .flex(1.f),
                                regularActionButton("Close", ButtonVariant::Secondary, hidePopover)
                            ),
                        },
                        .placement = placement,
                        .arrow = *showArrow,
                        .maxSize = Size {260.f, 200.f},
                        .backdropColor = Colors::transparent,
                        .dismissOnEscape = true,
                        .dismissOnOutsideTap = *dismissOutside,
                    });
                },
            });
        };

        addPlacementButton("Below", PopoverPlacement::Below);
        addPlacementButton("Above", PopoverPlacement::Above);
        addPlacementButton("End (right in LTR)", PopoverPlacement::End);
        addPlacementButton("Start (left in LTR)", PopoverPlacement::Start);

        addSection("Options");
        scrollChildren.push_back(HStack {
            .spacing = 12.f,
            .alignment = Alignment::Center,
            .children = children(
                Text {.text = "Arrow", .font = Font::headline(), .color = Color::primary()},
                Spacer {},
                Button {
                    .label = [showArrow] {
                        return showArrow() ? std::string {"On"} : std::string {"Off"};
                    },
                    .variant = ButtonVariant::Ghost,
                    .onTap = [=] { showArrow = !*showArrow; },
                }
            ),
        });
        scrollChildren.push_back(HStack {
            .spacing = 12.f,
            .alignment = Alignment::Center,
            .children = children(
                Text {.text = "Dismiss outside tap", .font = Font::headline(), .color = Color::primary()},
                Spacer {},
                Button {
                    .label = [dismissOutside] {
                        return dismissOutside() ? std::string {"On"} : std::string {"Off"};
                    },
                    .variant = ButtonVariant::Ghost,
                    .onTap = [=] { dismissOutside = !*dismissOutside; },
                }
            ),
        });

        addSection("Anchor tracking (scroll)");
        for (int i = 0; i < 8; ++i) {
            scrollChildren.push_back(
                Text {
                    .text = "Spacer row — scroll the list",
                    .font = Font::footnote(),
                    .color = Color::secondary(),
                }
                    .padding(6.f)
            );
        }
        scrollChildren.push_back(Button {
            .label = "Below — middle of scroll",
            .variant = ButtonVariant::Primary,
            .style = Button::Style {.accentColor = Color::accent()},
            .onTap = [=] {
                showPopover(Popover {
                    .content = Element {VStack {
                        .spacing = 8.f,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text {
                                .text = "Popover anchored to this button.",
                                .font = Font::title2(),
                                .color = Color::primary(),
                                .wrapping = TextWrapping::Wrap,
                            },
                            Text {
                                .text = "ScrollView keeps layout rects updated; anchor follows the trigger.",
                                .font = Font::footnote(),
                                .color = Color::secondary(),
                                .wrapping = TextWrapping::Wrap,
                            },
                            regularActionButton("OK", ButtonVariant::Primary, hidePopover)
                        ),
                    }},
                    .placement = PopoverPlacement::Below,
                    .arrow = *showArrow,
                    .maxSize = Size {280.f, 220.f},
                    .backdropColor = Colors::transparent,
                    .dismissOnEscape = true,
                    .dismissOnOutsideTap = *dismissOutside,
                });
            },
        });
        for (int i = 0; i < 8; ++i) {
            scrollChildren.push_back(
                Text {
                    .text = "Spacer row — scroll the list",
                    .font = Font::footnote(),
                    .color = Color::secondary(),
                }
                    .padding(6.f)
            );
        }
        scrollChildren.push_back(Button {
            .label = "Below — near bottom (may flip Above)",
            .variant = ButtonVariant::Primary,
            .style = Button::Style {.accentColor = Color::accent()},
            .onTap = [=] {
                showPopover(Popover {
                    .content = Element {VStack {
                        .spacing = 8.f,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text {.text = "Flip test", .font = Font::title2(), .color = Color::primary()},
                            HStack {
                                .spacing = 0.f,
                                .children = children(
                                    Text {
                                        .text = "If there is not enough room below the anchor, placement flips to Above.",
                                        .font = Font::footnote(),
                                        .color = Color::secondary(),
                                        .wrapping = TextWrapping::Wrap,
                                    }
                                        .flex(1.f)
                                ),
                            },
                            regularActionButton("OK", ButtonVariant::Primary, hidePopover)
                        ),
                    }},
                    .placement = PopoverPlacement::Below,
                    .arrow = *showArrow,
                    .maxSize = Size {280.f, 220.f},
                    .backdropColor = Colors::transparent,
                    .dismissOnEscape = true,
                    .dismissOnOutsideTap = *dismissOutside,
                });
            },
        });

        return VStack {
            .spacing = 0.f,
            .children = children(
                Text {
                    .text = "Popover demo",
                    .font = Font::largeTitle(),
                    .color = Color::primary(),
                    .horizontalAlignment = HorizontalAlignment::Center,
                }
                    .padding(16.f),
                Text {
                    .text = popoverOpen ? "Popover visible" : "Popover hidden",
                    .font = Font::headline(),
                    .color = Color::secondary(),
                    .horizontalAlignment = HorizontalAlignment::Center,
                }
                    .padding(8.f),
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = 10.f,
                            .alignment = Alignment::Stretch,
                            .children = std::move(scrollChildren),
                        }
                            .padding(20.f)
                    ),
                }
                    .flex(1.f, 1.f, 0.f)
                    .fill(FillStyle::solid(Color::elevatedBackground()))
                    .cornerRadius(CornerRadius {theme().radiusLarge})
            ),
        }
            .padding(20.f)
            .fill(FillStyle::solid(Color::windowBackground()));
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto &w = app.createWindow({
        .size = {800, 800},
        .title = "Flux — Popover demo",
    });

    w.setView(PopoverDemoRoot {});

    return app.exec();
}
