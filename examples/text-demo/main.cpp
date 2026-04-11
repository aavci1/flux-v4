#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <string>
#include <vector>

using namespace flux;

namespace {

char const *kWrapSample =
    "Flux uses the same text constraints for measurement and layout, so wrapped paragraphs reflow "
    "predictably as the window changes size.";

char const *kLongToken =
    "Supercalifragilisticexpialidocious_pseudopseudohypoparathyroidism_rendering_pipeline";

Element sectionCard(Theme const &theme, std::string title, std::string body, Element content) {
    return VStack {
        .spacing = theme.space3,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(title),
                .font = theme.fontTitle,
                .color = theme.colorTextPrimary,
            },
            Text {
                .text = std::move(body),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .wrapping = TextWrapping::Wrap,
            },
            std::move(content)
        )
    }
        .padding(theme.space4)
        .fill(FillStyle::solid(theme.colorSurfaceOverlay))
        .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
        .cornerRadius(theme.radiusLarge);
}

Element alignmentBand(Theme const &theme, std::string label, HorizontalAlignment alignment) {
    Element band = Element {ZStack {
                                .children = children(
                                    Rectangle {}
                                        .height(44.f)
                                        .fill(FillStyle::solid(theme.colorSurfaceHover))
                                        .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                                        .cornerRadius(theme.radiusMedium),
                                    Text {
                                        .text = "Alignment sample",
                                        .font = theme.fontBody,
                                        .color = theme.colorTextPrimary,
                                        .horizontalAlignment = alignment,
                                        .verticalAlignment = VerticalAlignment::Center,
                                    }
                                        .flex(1.f, 0.f, 0.f)
                                        .size(0.f, 44.f)
                                )
                            }}
                       .size(0.f, 44.f);

    return VStack {
        .spacing = theme.space2,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(label),
                .font = theme.fontLabelSmall,
                .color = theme.colorTextMuted,
            },
            std::move(band)
        )
    }
        .flex(1.f);
}

Element wrappingExamples(Theme const &theme) {
    return VStack {
        .spacing = theme.space3,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = "Wrap",
                .font = theme.fontLabel,
                .color = theme.colorTextPrimary,
            },
            Text {
                .text = kWrapSample,
                .font = theme.fontBody,
                .color = theme.colorTextPrimary,
                .wrapping = TextWrapping::Wrap,
            }
                .padding(theme.space3)
                .fill(FillStyle::solid(theme.colorSurfaceHover))
                .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                .cornerRadius(theme.radiusMedium),
            Text {
                .text = "NoWrap",
                .font = theme.fontLabel,
                .color = theme.colorTextPrimary,
            },
            Text {
                .text = kLongToken,
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .wrapping = TextWrapping::NoWrap,
            }
                .padding(theme.space3)
                .fill(FillStyle::solid(theme.colorSurfaceHover))
                .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                .cornerRadius(theme.radiusMedium),
            Text {
                .text = "WrapAnywhere",
                .font = theme.fontLabel,
                .color = theme.colorTextPrimary,
            },
            Text {
                .text = kLongToken,
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .wrapping = TextWrapping::WrapAnywhere,
            }
                .padding(theme.space3)
                .fill(FillStyle::solid(theme.colorSurfaceHover))
                .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                .cornerRadius(theme.radiusMedium)
        )
    };
}

} // namespace

struct TextDemoRoot {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        Element intro = VStack {
            .spacing = theme.space3,
            .alignment = Alignment::Start,
            .children = children(
                Text {
                    .text = "Text Demo",
                    .font = theme.fontDisplay,
                    .color = theme.colorTextPrimary,
                },
                Text {
                    .text = "A compact tour of wrapping, alignment, truncation, and semantic emphasis.",
                    .font = theme.fontBody,
                    .color = theme.colorTextSecondary,
                    .wrapping = TextWrapping::Wrap,
                }
            )
        };

        Element alignmentSection = sectionCard(
            theme, "Alignment", "The same text view can be positioned differently inside its layout box.",
            HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    alignmentBand(theme, "Leading", HorizontalAlignment::Leading),
                    alignmentBand(theme, "Center", HorizontalAlignment::Center),
                    alignmentBand(theme, "Trailing", HorizontalAlignment::Trailing)
                )
            }
        );

        Element wrappingSection =
            sectionCard(theme, "Wrapping Modes",
                        "These examples show the three supported wrapping behaviors under the same width.",
                        wrappingExamples(theme));

        Element maxLinesSection = sectionCard(
            theme, "Line Limits",
            "Use maxLines to keep a layout compact while leaving measurement and line geometry consistent.",
            VStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    Text {
                        .text = "Full paragraph",
                        .font = theme.fontLabel,
                        .color = theme.colorTextPrimary,
                    },
                    Text {
                        .text = std::string(kWrapSample) + " " + kWrapSample,
                        .font = theme.fontBody,
                        .color = theme.colorTextPrimary,
                        .wrapping = TextWrapping::Wrap,
                    }
                        .padding(theme.space3)
                        .fill(FillStyle::solid(theme.colorSurfaceHover))
                        .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                        .cornerRadius(theme.radiusMedium),
                    Text {
                        .text = "Same text with maxLines = 2",
                        .font = theme.fontLabel,
                        .color = theme.colorTextPrimary,
                    },
                    Text {
                        .text = std::string(kWrapSample) + " " + kWrapSample,
                        .font = theme.fontBody,
                        .color = theme.colorTextPrimary,
                        .wrapping = TextWrapping::Wrap,
                        .maxLines = 2,
                    }
                        .padding(theme.space3)
                        .fill(FillStyle::solid(theme.colorSurfaceHover))
                        .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                        .cornerRadius(theme.radiusMedium)
                )
            }
        );

        Element emphasisSection = sectionCard(
            theme, "Semantic Emphasis",
            "Text styles and semantic colors are meant to be mixed deliberately, not just resized mechanically.",
            HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    VStack {
                        .spacing = theme.space2,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text {.text = "Primary", .font = theme.fontBody, .color = theme.colorTextPrimary},
                            Text {.text = "Secondary", .font = theme.fontBody, .color = theme.colorTextSecondary},
                            Text {.text = "Muted", .font = theme.fontBody, .color = theme.colorTextMuted}
                        )
                    }
                        .flex(1.f),
                    VStack {
                        .spacing = theme.space2,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text {.text = "Accent", .font = theme.fontBody, .color = theme.colorAccent},
                            Text {.text = "Success", .font = theme.fontBody, .color = theme.colorSuccess},
                            Text {.text = "Danger", .font = theme.fontBody, .color = theme.colorDanger}
                        )
                    }
                        .flex(1.f)
                )
            }
        );

        Element content = VStack {
            .spacing = theme.space5,
            .alignment = Alignment::Start,
            .children = children(
                std::move(intro),
                std::move(alignmentSection),
                std::move(wrappingSection),
                std::move(maxLinesSection),
                std::move(emphasisSection)
            )
        }
                              .padding(theme.space5);

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(std::move(content)),
        }
            .fill(FillStyle::solid(theme.colorBackground));
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto &w = app.createWindow<Window>({
        .size = {860, 760},
        .title = "Flux — Text Demo",
        .resizable = true,
    });

    w.setView<TextDemoRoot>();
    return app.exec();
}
