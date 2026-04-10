#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace flux;

namespace {

Element makeSectionCard(Theme const &theme, std::string title, std::string caption, Element content) {
    return VStack {
        .spacing = theme.space3,
        .children = children(
            Text {
                .text = std::move(title),
                .font = theme.fontTitle,
                .color = theme.colorTextPrimary,
                .horizontalAlignment = HorizontalAlignment::Leading,
            },
            Text {
                .text = std::move(caption),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .horizontalAlignment = HorizontalAlignment::Leading,
            },
            std::move(content)
        )
    }
        .padding(theme.space4)
        .fill(FillStyle::solid(theme.colorSurfaceOverlay))
        .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
        .cornerRadius(CornerRadius {theme.radiusLarge});
}

Element makeVerticalDemo(Theme const &theme) {
    std::vector<Element> rows;
    rows.reserve(24);

    for (int i = 1; i <= 24; ++i) {
        std::ostringstream title;
        title << "Activity row " << i;
        std::ostringstream body;
        body << "Vertical scrolling should only show the trailing indicator even "
                "if this line stretches a bit wider than the viewport.";

        rows.push_back(
            VStack {
                .spacing = 4.f,
                .children = children(
                    Text {
                        .text = title.str(),
                        .font = theme.fontLabel,
                        .color = theme.colorTextPrimary,
                        .horizontalAlignment = HorizontalAlignment::Leading,
                    },
                    Text {
                        .text = body.str(),
                        .font = theme.fontBodySmall,
                        .color = theme.colorTextSecondary,
                        .horizontalAlignment = HorizontalAlignment::Leading,
                    }
                )
            }
                .padding(theme.space3)
                .fill(FillStyle::solid(i % 2 == 0 ? theme.colorSurface : theme.colorSurfaceHover))
                .cornerRadius(CornerRadius {theme.radiusMedium})
        );
    }

    return makeSectionCard(
        theme, "Vertical",
        "Wheel, trackpad, or drag to scroll. Horizontal overflow should not "
        "advertise a horizontal indicator here.",
        ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(VStack {
                .spacing = theme.space2,
                .children = std::move(rows),
            })
        }
            .height(220.f)
            .fill(FillStyle::solid(theme.colorBackground))
            .cornerRadius(CornerRadius {theme.radiusMedium})
    );
}

Element makeHorizontalDemo(Theme const &theme) {
    std::vector<Element> cards;
    cards.reserve(10);

    for (int i = 1; i <= 10; ++i) {
        std::ostringstream title;
        title << "Lane " << i;
        std::ostringstream body;
        body << "Horizontal content card " << i
             << " with enough width to force scrolling.";

        cards.push_back(
            VStack {
                .spacing = theme.space2,
                .children = children(
                    Text {
                        .text = title.str(),
                        .font = theme.fontLabel,
                        .color = theme.colorTextOnAccent,
                        .horizontalAlignment = HorizontalAlignment::Leading,
                    },
                    Text {
                        .text = body.str(),
                        .font = theme.fontBodySmall,
                        .color = theme.colorTextOnAccent,
                        .horizontalAlignment = HorizontalAlignment::Leading,
                        .wrapping = TextWrapping::Wrap,
                    }
                )
            }
                .padding(theme.space4)
                .width(220.f)
                .fill(FillStyle::solid(i % 2 == 0 ? theme.colorAccent : theme.colorSuccess))
                .cornerRadius(CornerRadius {theme.radiusLarge})
        );
    }

    return makeSectionCard(
        theme, "Horizontal",
        "A single row of fixed-width cards exercises the "
        "bottom indicator and horizontal dragging.",
        ScrollView {
            .axis = ScrollAxis::Horizontal,
            .children = children(
                HStack {
                    .spacing = theme.space3,
                    .children = std::move(cards),
                }
                    .padding(theme.space3)
            )
        }
            .fill(FillStyle::solid(theme.colorBackground))
            .cornerRadius(CornerRadius {theme.radiusMedium})
    );
}

Element makeBothAxesDemo(Theme const &theme) {
    std::vector<Element> columns;
    columns.reserve(6);

    for (int col = 0; col < 6; ++col) {
        std::vector<Element> cells;
        cells.reserve(7);

        for (int row = 0; row < 7; ++row) {
            std::ostringstream label;
            label << "Cell " << (row + 1) << ":" << (col + 1);
            cells.push_back(
                VStack {
                    .spacing = theme.space1,
                    .children = children(
                        Text {
                            .text = label.str(),
                            .font = theme.fontLabelSmall,
                            .color = theme.colorTextPrimary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Rectangle {}
                            .size(140.f, 28.f + static_cast<float>((row % 3) * 12))
                            .cornerRadius(CornerRadius {theme.radiusSmall})
                            .fill(FillStyle::solid(row % 2 == 0 ? theme.colorWarningSubtle : theme.colorAccentSubtle))
                    )
                }
                    .padding(theme.space2)
                    .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                    .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                    .cornerRadius(CornerRadius {theme.radiusMedium})
            );
        }

        std::ostringstream title;
        title << "Column " << (col + 1);
        columns.push_back(VStack {
            .spacing = theme.space2,
            .children = children(
                Text {
                    .text = title.str(),
                    .font = theme.fontSubtitle,
                    .color = theme.colorTextPrimary,
                    .horizontalAlignment = HorizontalAlignment::Leading,
                },
                VStack {
                    .spacing = theme.space2,
                    .children = std::move(cells),
                }
            )
        }
                              .padding(theme.space3)
                              .width(210.f)
                              .fill(FillStyle::solid(theme.colorSurface))
                              .cornerRadius(CornerRadius {theme.radiusLarge}));
    }

    return makeSectionCard(
        theme, "Both Axes",
        "This oversized canvas should keep both indicators pinned to the "
        "viewport edges while the content moves underneath.",
        ScrollView {
            .axis = ScrollAxis::Both,
            .children = children(
                HStack {
                    .spacing = theme.space3,
                    .children = std::move(columns),
                }
                    .padding(theme.space3)
            )
        }
            .height(260.f)
            .fill(FillStyle::solid(theme.colorBackground))
            .cornerRadius(CornerRadius {theme.radiusMedium})
    );
}

} // namespace

struct ScrollDemoRoot {
    Element body() const {
        Theme const &theme = useEnvironment<Theme>();

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = theme.space4,
                    .children = children(
                        Text {
                            .text = "ScrollView Demo",
                            .font = theme.fontDisplay,
                            .color = theme.colorTextPrimary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Text {
                            .text = "Three focused examples for vertical, horizontal, "
                                    "and two-axis scrolling with overlay indicators.",
                            .font = theme.fontBody,
                            .color = theme.colorTextSecondary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        makeVerticalDemo(theme),
                        makeHorizontalDemo(theme),
                        makeBothAxesDemo(theme)
                    )
                }
                    .padding(theme.space5)
            )
        }
            .fill(FillStyle::solid(theme.colorBackground));
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto &w = app.createWindow({
        .size = {960, 920},
        .title = "Flux - Scroll demo",
    });

    w.setView<ScrollDemoRoot>();

    return app.exec();
}
