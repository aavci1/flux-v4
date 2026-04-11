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

using namespace flux;

namespace {

Element section(Theme const &theme, std::string title, std::string body, Element content) {
    return VStack {
        .spacing = theme.space3,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(title),
                .font = theme.fontHeading,
                .color = theme.colorTextPrimary,
            },
            Text {
                .text = std::move(body),
                .font = theme.fontBody,
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

Element typeRow(Theme const &theme, std::string tokenName, Font const &font, std::string sample) {
    return HStack {
        .spacing = theme.space3,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(tokenName),
                .font = theme.fontLabelSmall,
                .color = theme.colorTextMuted,
            }
                .size(130.f, 0.f),
            Text {
                .text = std::move(sample),
                .font = font,
                .color = theme.colorTextPrimary,
                .wrapping = TextWrapping::Wrap,
            }
                .flex(1.f)
        )
    };
}

Element colorTile(Theme const &theme, std::string name, Color swatch, Color textColor, std::string note) {
    return VStack {
        .spacing = theme.space2,
        .alignment = Alignment::Start,
        .children = children(
            Rectangle {}
                .height(46.f)
                .fill(FillStyle::solid(swatch))
                .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                .cornerRadius(theme.radiusMedium),
            Text {
                .text = std::move(name),
                .font = theme.fontLabel,
                .color = textColor,
            },
            Text {
                .text = std::move(note),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .wrapping = TextWrapping::Wrap,
            }
        )
    }
        .flex(1.f);
}

} // namespace

struct TypographyDemoRoot {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        Element scaleSection = section(
            theme, "Theme Scale",
            "Each token has a role. The point is consistency of meaning, not just different font sizes.",
            VStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    typeRow(theme, "fontDisplay", theme.fontDisplay, "Screen or hero headline"),
                    typeRow(theme, "fontHeading", theme.fontHeading, "Major section heading"),
                    typeRow(theme, "fontTitle", theme.fontTitle, "Card, modal, or nested panel title"),
                    typeRow(theme, "fontSubtitle", theme.fontSubtitle, "Grouped subsection heading"),
                    typeRow(theme, "fontBody", theme.fontBody, "Primary reading text for paragraphs and descriptions."),
                    typeRow(theme, "fontBodySmall", theme.fontBodySmall, "Supporting text, captions, and metadata."),
                    typeRow(theme, "fontLabel", theme.fontLabel, "Control labels and compact headers"),
                    typeRow(theme, "fontLabelSmall", theme.fontLabelSmall, "Footnotes and dense UI labels"),
                    typeRow(theme, "fontCode", theme.fontCode, "cache_key = typography_demo")
                )
            }
        );

        Element toneSection = section(
            theme, "Semantic Tone",
            "The semantic colors below should communicate hierarchy and intent before any icon or shape does.",
            HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    colorTile(theme, "Primary", theme.colorTextPrimary, theme.colorTextPrimary,
                              "Main ink for titles and body copy."),
                    colorTile(theme, "Secondary", theme.colorTextSecondary, theme.colorTextSecondary,
                              "Supportive descriptions and less prominent text."),
                    colorTile(theme, "Muted", theme.colorTextMuted, theme.colorTextMuted,
                              "Hints, metadata, and tertiary detail.")
                )
            }
        );

        Element accentSection = section(
            theme, "Accent States",
            "Accent, success, warning, and danger should feel like semantic shifts, not random color changes.",
            HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    colorTile(theme, "Accent", theme.colorAccent, theme.colorAccent,
                              "Interactive emphasis and active states."),
                    colorTile(theme, "Success", theme.colorSuccess, theme.colorSuccess,
                              "Positive outcomes and confirmations."),
                    colorTile(theme, "Warning", theme.colorWarning, theme.colorWarning,
                              "Cautionary context without full failure."),
                    colorTile(theme, "Danger", theme.colorDanger, theme.colorDanger,
                              "Destructive actions and error context.")
                )
            }
        );

        Element compositionSection = section(
            theme, "Composition Example",
            "A good screen usually mixes only a few levels of type at once: one headline, one body voice, and one supporting voice.",
            VStack {
                .spacing = theme.space2,
                .alignment = Alignment::Start,
                .children = children(
                    Text {
                        .text = "Project Overview",
                        .font = theme.fontTitle,
                        .color = theme.colorTextPrimary,
                    },
                    Text {
                        .text = "Typography feels polished when the hierarchy is obvious at a glance and calm to read for long stretches.",
                        .font = theme.fontBody,
                        .color = theme.colorTextPrimary,
                        .wrapping = TextWrapping::Wrap,
                    },
                    Text {
                        .text = "Updated 12 minutes ago by Design Systems",
                        .font = theme.fontBodySmall,
                        .color = theme.colorTextSecondary,
                        .wrapping = TextWrapping::Wrap,
                    },
                    Text {
                        .text = "Stable",
                        .font = theme.fontLabel,
                        .color = theme.colorSuccess,
                    }
                )
            }
        );

        Element content = VStack {
            .spacing = theme.space5,
            .alignment = Alignment::Start,
            .children = children(
                Text {
                    .text = "Typography Demo",
                    .font = theme.fontDisplay,
                    .color = theme.colorTextPrimary,
                },
                Text {
                    .text = "A clean reference for the theme text scale, semantic colors, and tone.",
                    .font = theme.fontBody,
                    .color = theme.colorTextSecondary,
                    .wrapping = TextWrapping::Wrap,
                },
                std::move(scaleSection),
                std::move(toneSection),
                std::move(accentSection),
                std::move(compositionSection)
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
        .size = {900, 820},
        .title = "Flux — Typography Demo",
        .resizable = true,
    });

    w.setView<TypographyDemoRoot>();
    return app.exec();
}
