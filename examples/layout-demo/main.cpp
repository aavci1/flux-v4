#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Card.hpp>
#include <Flux/UI/Views/Grid.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace flux;

namespace {

Element makeSectionCard(Theme const &theme, std::string title, std::string caption, Element content) {
    return Card {
        .child = VStack {
            .spacing = theme.space3,
            .alignment = Alignment::Start,
            .children = children(
                Text {
                    .text = std::move(title),
                    .font = Font::title2(),
                    .color = Color::primary(),
                    .horizontalAlignment = HorizontalAlignment::Leading,
                },
                Text {
                    .text = std::move(caption),
                    .font = Font::footnote(),
                    .color = Color::secondary(),
                    .horizontalAlignment = HorizontalAlignment::Leading,
                    .wrapping = TextWrapping::Wrap,
                },
                std::move(content)
            )
        },
        .style = Card::Style {
            .padding = theme.space4,
            .cornerRadius = theme.radiusLarge,
        },
    };
}

Element colorBlock(Color color, float width, float height, float radius) {
    return Rectangle {}
        .size(width, height)
        .fill(FillStyle::solid(color))
        .cornerRadius(CornerRadius {radius});
}

Element makeVStackDemo(Theme const &theme) {
    return makeSectionCard(
        theme, "VStack",
        "Children flow top-to-bottom. Center alignment keeps each child at its intrinsic width and centers it in the column.",
        VStack {
            .spacing = theme.space3,
            .alignment = Alignment::Center,
            .children = children(
                colorBlock(Color::accent(), 160.f, 34.f, theme.radiusMedium),
                colorBlock(Color::success(), 220.f, 42.f, theme.radiusMedium),
                colorBlock(Color::warning(), 120.f, 30.f, theme.radiusMedium),
                HStack {
                    .spacing = theme.space2,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = "Rows can also host nested stacks",
                            .font = Font::headline(),
                            .color = Color::primary(),
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Spacer {},
                        Text {
                            .text = "nested",
                            .font = Font::caption(),
                            .color = Color::secondary(),
                            .horizontalAlignment = HorizontalAlignment::Trailing,
                        }
                    ),
                }
                    .padding(theme.space3)
                    .fill(FillStyle::solid(Color::controlBackground()))
                    .cornerRadius(CornerRadius {theme.radiusMedium})
            )
        } //
            .padding(theme.space3)
            .fill(FillStyle::solid(Color::windowBackground()))
            .cornerRadius(CornerRadius {theme.radiusMedium})
    );
}

Element makeHStackDemo(Theme const &theme) {
    return makeSectionCard(
        theme, "HStack",
        "Children flow left-to-right. Flex growth lets selected items absorb remaining width.",
        VStack {
            .spacing = theme.space3,
            .children = children(
                HStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        colorBlock(Color::accent(), 56.f, 54.f, theme.radiusMedium).flex(2.f, 1.f, 0.f),
                        colorBlock(Color::success(), 56.f, 76.f, theme.radiusMedium),
                        colorBlock(Color::warning(), 56.f, 40.f, theme.radiusMedium).flex(1.f, 1.f, 0.f),
                        colorBlock(Color::danger(), 56.f, 54.f, theme.radiusMedium)
                    ),
                },
                HStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = "Leading",
                            .font = Font::headline(),
                            .color = Color::primary(),
                        },
                        Spacer {},
                        Text {
                            .text = "Spacer pushes this trailing label",
                            .font = Font::caption(),
                            .color = Color::secondary(),
                            .horizontalAlignment = HorizontalAlignment::Trailing,
                        }
                    ),
                }
                    .padding(theme.space3)
                    .fill(FillStyle::solid(Color::controlBackground()))
                    .cornerRadius(CornerRadius {theme.radiusMedium})
            )
        } //
            .padding(theme.space3)
            .fill(FillStyle::solid(Color::windowBackground()))
            .cornerRadius(CornerRadius {theme.radiusMedium})
    );
}

Element makeZStackDemo(Theme const &theme) {
    return makeSectionCard(
        theme, "ZStack",
        "Children share the same space. This is useful for overlays, badges, and stacked decoration.",
        ZStack {
            .horizontalAlignment = Alignment::Center,
            .verticalAlignment = Alignment::Center,
            .children = children(
                Rectangle {}
                    .size(0.f, 180.f)
                    .fill(FillStyle::solid(Color::selectedContentBackground()))
                    .cornerRadius(CornerRadius {theme.radiusLarge}),
                Rectangle {}
                    .size(220.f, 104.f)
                    .fill(FillStyle::solid(Color::accent()))
                    .cornerRadius(CornerRadius {theme.radiusLarge}),
                VStack {
                    .spacing = theme.space1,
                    .children = children(
                        Text {
                            .text = "Overlay content",
                            .font = Font::title2(),
                            .color = Color::accentForeground(),
                            .horizontalAlignment = HorizontalAlignment::Center,
                        },
                        Text {
                            .text = "Centered inside a shared layer",
                            .font = Font::footnote(),
                            .color = Color::accentForeground(),
                            .horizontalAlignment = HorizontalAlignment::Center,
                        }
                    )
                }
            )
        }
            .padding(theme.space3)
            .fill(FillStyle::solid(Color::windowBackground()))
            .cornerRadius(CornerRadius {theme.radiusMedium})
    );
}

Element makeGridDemo(Theme const &theme) {
    std::vector<Element> cells;
    cells.reserve(8);

    std::vector<Color> palette = {
        Color::accent(),
        Color::success(),
        Color::warning(),
        Color::danger(),
        Color::selectedContentBackground(),
        Color::successBackground(),
        Color::warningBackground(),
        theme.hoveredControlBackgroundColor,
    };

    for (int i = 0; i < 8; ++i) {
        std::ostringstream title;
        title << "Cell " << (i + 1);
        cells.push_back(
            VStack {
                .spacing = theme.space1,
                .alignment = Alignment::Start,
                .children = children(
                    Text {
                        .text = title.str(),
                        .font = Font::caption(),
                        .color = i < 4 ? Color::accentForeground() : Color::primary(),
                        .horizontalAlignment = HorizontalAlignment::Leading,
                    },
                    Rectangle {}
                        .size(24.f + static_cast<float>((i % 3) * 18), 18.f + static_cast<float>((i % 2) * 12))
                        .fill(FillStyle::solid(i < 4 ? Color::accentForeground() : Color::secondary()))
                        .cornerRadius(CornerRadius {theme.radiusSmall})
                )
            } //
                .padding(theme.space3)
                .fill(FillStyle::solid(palette[static_cast<std::size_t>(i)]))
                .cornerRadius(CornerRadius {theme.radiusMedium})
        );
    }

    return makeSectionCard(
        theme, "Grid",
        "Fixed columns place children row-by-row. Mixed intrinsic sizes stay aligned inside each cell.",
        Grid {
            .columns = 3,
            .horizontalSpacing = theme.space3,
            .verticalSpacing = theme.space3,
            .horizontalAlignment = Alignment::Center,
            .verticalAlignment = Alignment::Center,
            .children = std::move(cells),
        }
            .padding(theme.space3)
            .fill(FillStyle::solid(Color::windowBackground()))
            .cornerRadius(CornerRadius {theme.radiusMedium})
    );
}

Element makeMixedCompositionDemo(Theme const &theme) {
    std::vector<Element> rows;
    rows.reserve(4);

    for (int i = 0; i < 4; ++i) {
        std::ostringstream label;
        label << "Track " << (i + 1);
        rows.push_back(HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Center,
            .children = children(
                Text {
                    .text = label.str(),
                    .font = Font::headline(),
                    .color = Color::primary(),
                    .horizontalAlignment = HorizontalAlignment::Leading,
                }
                    .width(72.f),
                Rectangle {}
                    .height(14.f)
                    .fill(FillStyle::solid(i % 2 == 0 ? Color::selectedContentBackground() : Color::successBackground()))
                    .cornerRadius(CornerRadius {theme.radiusSmall})
                    .flex(1.f, 1.f, 0.f),
                Text {
                    .text = i % 2 == 0 ? "auto" : "manual",
                    .font = Font::caption(),
                    .color = Color::secondary(),
                    .horizontalAlignment = HorizontalAlignment::Trailing,
                }
            )
        } //
                           .padding(theme.space2)
                           .fill(FillStyle::solid(Color::controlBackground()))
                           .cornerRadius(CornerRadius {theme.radiusMedium}));
    }

    return makeSectionCard(
        theme, "Composed Layout",
        "Real views usually mix stacks together. This section shows a common label-track-value row pattern.",
        VStack {
            .spacing = theme.space2,
            .children = std::move(rows)
        } //
            .padding(theme.space3)
            .fill(FillStyle::solid(Color::windowBackground()))
            .cornerRadius(CornerRadius {theme.radiusMedium})
    );
}

} // namespace

struct StackDemoRoot {
    Element body() const {
        Theme const &theme = useEnvironment<Theme>();

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = theme.space4,
                    .alignment = Alignment::Start,
                    .children = children(
                        Text {
                            .text = "Layout Demo",
                            .font = Font::largeTitle(),
                            .color = Color::primary(),
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Text {
                            .text =
                                "Focused examples for VStack, HStack, ZStack, Grid, and how they compose in practice.",
                            .font = Font::body(),
                            .color = Color::secondary(),
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        },
                        makeVStackDemo(theme),
                        makeHStackDemo(theme),
                        makeZStackDemo(theme),
                        makeGridDemo(theme),
                        makeMixedCompositionDemo(theme)
                    )
                } //
                    .padding(theme.space5)
            )
        } //
            .fill(FillStyle::solid(Color::windowBackground()));
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto &w = app.createWindow({
        .size = {800, 800},
        .title = "Flux - Layout demo",
    });

    w.setView<StackDemoRoot>();

    return app.exec();
}
