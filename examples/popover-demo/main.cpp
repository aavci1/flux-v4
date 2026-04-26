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

#include <string>
#include <vector>

using namespace flux;

struct PopoverDemoRoot {
    auto body() const {
        auto showArrow = useState<bool>(true);
        auto dismissOutside = useState<bool>(true);
        auto [showPopover, hidePopover, popoverOpen] = usePopover();

        Theme const &theme = useEnvironment<Theme>();

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
                                Button {
                                    .label = "Close",
                                    .variant = ButtonVariant::Secondary,
                                    .onTap = hidePopover,
                                }
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
                    .label = Reactive::Bindable<std::string> {[showArrow] {
                        return showArrow.get() ? std::string {"On"} : std::string {"Off"};
                    }},
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
                    .label = Reactive::Bindable<std::string> {[dismissOutside] {
                        return dismissOutside.get() ? std::string {"On"} : std::string {"Off"};
                    }},
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
                            Text {.text = "Popover anchored to this button.", .font = Font::title2(), .color = Color::primary()},
                            HStack {
                                .spacing = 0.f,
                                .children = children(
                                    Text {
                                        .text = "ScrollView keeps layout rects updated; anchor follows the trigger.",
                                        .font = Font::footnote(),
                                        .color = Color::secondary(),
                                        .wrapping = TextWrapping::Wrap,
                                    }
                                        .flex(1.f)
                                ),
                            },
                            Button {.label = "OK", .onTap = hidePopover}
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
                            Button {.label = "OK", .onTap = hidePopover}
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
                    .cornerRadius(CornerRadius {theme.radiusLarge})
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
