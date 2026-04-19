#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>

using namespace flux;

namespace {

Element dot(Color color) {
    return Rectangle {}
        .size(10.f, 10.f)
        .fill(color)
        .cornerRadius(999.f);
}

Element sectionCard(Theme const &theme, std::string eyebrow, std::string title, std::string body, Element content) {
    return VStack {
        .spacing = theme.space3,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(eyebrow),
                .font = Font::caption(),
                .color = Color::accent(),
            },
            Text {
                .text = std::move(title),
                .font = Font::title2(),
                .color = Color::primary(),
            },
            Text {
                .text = std::move(body),
                .font = Font::body(),
                .color = Color::secondary(),
                .wrapping = TextWrapping::Wrap,
            },
            std::move(content)
        )
    }
        .padding(theme.space4)
        .fill(Color::elevatedBackground())
        .stroke(Color::separator(), 1.f)
        .cornerRadius(theme.radiusXLarge)
        .shadow(ShadowStyle {.radius = theme.shadowRadiusPopover, .offset = {0.f, theme.shadowOffsetYPopover}, .color = Color {0.f, 0.f, 0.f, 0.08f}});
}

Element typeRow(Theme const &theme, std::string tokenName, Font token, std::string note) {
    return HStack {
        .spacing = theme.space3,
        .alignment = Alignment::Start,
        .children = children(
            VStack {
                .spacing = theme.space1,
                .alignment = Alignment::Start,
                .children = children(
                    Text {
                        .text = std::move(tokenName),
                        .font = Font::monospacedBody(),
                        .color = Color::tertiary(),
                    },
                    Text {
                        .text = std::move(note),
                        .font = Font::caption(),
                        .color = Color::secondary(),
                        .wrapping = TextWrapping::Wrap,
                    }
                )
            }
                .size(210.f, 0.f),
            Text {
                .text = "The quick brown fox jumps over the lazy dog.",
                .font = token,
                .color = Color::primary(),
                .wrapping = TextWrapping::Wrap,
            }
                .flex(1.f)
        )
    };
}

Element swatchTile(Theme const &theme, std::string name, Color swatch, std::string note, Color sampleText = Color::primary()) {
    return VStack {
        .spacing = theme.space2,
        .alignment = Alignment::Start,
        .children = children(
            Rectangle {}
                .height(48.f)
                .fill(swatch)
                .stroke(Color::separator(), 1.f)
                .cornerRadius(theme.radiusMedium),
            Text {
                .text = std::move(name),
                .font = Font::headline(),
                .color = sampleText,
            },
            Text {
                .text = std::move(note),
                .font = Font::footnote(),
                .color = Color::secondary(),
                .wrapping = TextWrapping::Wrap,
            }
        )
    }
        .flex(1.f);
}

Element previewWindow(Theme previewTheme, std::string name, std::string note) {
    Theme const &theme = useEnvironment<Theme>();

    Element content = VStack {
        .spacing = theme.space3,
        .alignment = Alignment::Start,
        .children = children(
            HStack {
                .spacing = theme.space2,
                .alignment = Alignment::Center,
                .children = children(
                    dot(Color::danger()),
                    dot(Color::warning()),
                    dot(Color::success()),
                    Text {
                        .text = std::move(name),
                        .font = Font::headline(),
                        .color = Color::secondary(),
                    }
                )
            },
            VStack {
                .spacing = theme.space1,
                .alignment = Alignment::Start,
                .children = children(
                    Text {
                        .text = "Semantic Theme",
                        .font = Font::title3(),
                        .color = Color::primary(),
                    },
                    Text {
                        .text = std::move(note),
                        .font = Font::body(),
                        .color = Color::secondary(),
                        .wrapping = TextWrapping::Wrap,
                    }
                )
            },
            VStack {
                .spacing = theme.space2,
                .alignment = Alignment::Start,
                .children = children(
                    Text {
                        .text = "Selected row",
                        .font = Font::headline(),
                        .color = Color::primary(),
                    }
                        .padding(theme.space3)
                        .fill(Color::selectedContentBackground())
                        .cornerRadius(theme.radiusMedium),
                    Text {
                        .text = "Placeholder",
                        .font = Font::body(),
                        .color = Color::placeholder(),
                    }
                        .padding(theme.space3)
                        .fill(Color::textBackground())
                        .stroke(Color::opaqueSeparator(), 1.f)
                        .cornerRadius(theme.radiusMedium),
                    HStack {
                        .spacing = theme.space2,
                        .alignment = Alignment::Center,
                        .children = children(
                            Text {
                                .text = "Continue",
                                .font = Font::headline(),
                                .color = Color::accentForeground(),
                            }
                                .padding(theme.space3)
                                .fill(Color::accent())
                                .cornerRadius(theme.radiusMedium),
                            Text {
                                .text = "Sync complete",
                                .font = Font::footnote(),
                                .color = Color::success(),
                            }
                        )
                    }
                )
            }
        )
    }
        .padding(theme.space4)
        .fill(Color::controlBackground())
        .stroke(Color::separator(), 1.f)
        .cornerRadius(theme.radiusXLarge)
        .environment(previewTheme);

    return std::move(content).flex(1.f);
}

} // namespace

struct TypographyDemoRoot {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        Element previews = HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Start,
            .children = children(
                previewWindow(Theme::light(), "Light Appearance", "Primary and secondary content should feel calm and legible."),
                previewWindow(Theme::dark(), "Dark Appearance", "The same semantic tokens should keep hierarchy intact at night.")
            )
        };

        Element scaleSection = sectionCard(
            theme, "Typography", "Apple-style text roles",
            "These samples render exclusively through semantic `Font::...` tokens. The theme decides the concrete face, size, and weight later.",
            VStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    typeRow(theme, "Font::largeTitle()", Font::largeTitle(), "Screen headline"),
                    typeRow(theme, "Font::title()", Font::title(), "Primary section title"),
                    typeRow(theme, "Font::title2()", Font::title2(), "Panel or card title"),
                    typeRow(theme, "Font::title3()", Font::title3(), "Subsection title"),
                    typeRow(theme, "Font::headline()", Font::headline(), "Control label or emphasized row"),
                    typeRow(theme, "Font::subheadline()", Font::subheadline(), "Supporting hierarchy"),
                    typeRow(theme, "Font::body()", Font::body(), "Default reading text"),
                    typeRow(theme, "Font::callout()", Font::callout(), "Compact callout copy"),
                    typeRow(theme, "Font::footnote()", Font::footnote(), "Metadata and support text"),
                    typeRow(theme, "Font::caption()", Font::caption(), "Dense UI label"),
                    typeRow(theme, "Font::caption2()", Font::caption2(), "Tight caption"),
                    typeRow(theme, "Font::monospacedBody()", Font::monospacedBody(), "system.token = semantic")
                )
            }
        );

        Element textColors = sectionCard(
            theme, "Color", "Semantic text colors",
            "These follow the macOS model: content asks for meaning like primary or placeholder, not a concrete hex value.",
            HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    swatchTile(theme, "Color::primary()", Color::primary(), "Main reading ink."),
                    swatchTile(theme, "Color::secondary()", Color::secondary(), "Supporting descriptions.", Color::secondary()),
                    swatchTile(theme, "Color::tertiary()", Color::tertiary(), "Metadata and quiet detail.", Color::tertiary()),
                    swatchTile(theme, "Color::placeholder()", Color::placeholder(), "Transient hints before input.", Color::placeholder())
                )
            }
        );

        Element surfaces = sectionCard(
            theme, "Surfaces", "Background and separation",
            "Window, control, elevated, and text surfaces stay distinct without hard-coding different palettes in each component.",
            HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    swatchTile(theme, "Color::windowBackground()", Color::windowBackground(), "Canvas and app backdrop."),
                    swatchTile(theme, "Color::controlBackground()", Color::controlBackground(), "Cards and panels."),
                    swatchTile(theme, "Color::elevatedBackground()", Color::elevatedBackground(), "Raised surfaces like sheets."),
                    swatchTile(theme, "Color::textBackground()", Color::textBackground(), "Fields and editable regions.")
                )
            }
        );

        Element states = sectionCard(
            theme, "States", "Accent, focus, and feedback",
            "Interactive and status colors stay semantic too, including fills that now resolve late in the scene graph.",
            VStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    HStack {
                        .spacing = theme.space3,
                        .alignment = Alignment::Start,
                        .children = children(
                            swatchTile(theme, "Color::accent()", Color::accent(), "Primary interaction tint.", Color::accent()),
                            swatchTile(theme, "Color::selectedContentBackground()", Color::selectedContentBackground(), "Selected rows and ranges."),
                            swatchTile(theme, "Color::focusRing()", Color::focusRing(), "Keyboard focus affordance.", Color::focusRing()),
                            swatchTile(theme, "Color::separator()", Color::separator(), "Subtle chrome boundaries.", Color::secondary())
                        )
                    },
                    HStack {
                        .spacing = theme.space3,
                        .alignment = Alignment::Start,
                        .children = children(
                            swatchTile(theme, "Color::success()", Color::success(), "Positive confirmation.", Color::success()),
                            swatchTile(theme, "Color::warning()", Color::warning(), "Caution without failure.", Color::warning()),
                            swatchTile(theme, "Color::danger()", Color::danger(), "Destructive or failed state.", Color::danger()),
                            swatchTile(theme, "Color::scrim()", Color::scrim(), "Modal backdrop tone.", Color::secondary())
                        )
                    }
                )
            }
        );

        Element content = VStack {
            .spacing = theme.space5,
            .alignment = Alignment::Start,
            .children = children(
                Text {
                    .text = "Semantic Theme Tokens",
                    .font = Font::largeTitle(),
                    .color = Color::primary(),
                },
                Text {
                    .text = "A macOS-inspired first pass: late-resolved colors and fonts, one vocabulary for light and dark, and a demo that renders only through `Color::...` and `Font::...`.",
                    .font = Font::body(),
                    .color = Color::secondary(),
                    .wrapping = TextWrapping::Wrap,
                },
                std::move(previews),
                std::move(scaleSection),
                std::move(textColors),
                std::move(surfaces),
                std::move(states)
            )
        }
            .padding(theme.space5);

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(std::move(content)),
        }
            .fill(Color::windowBackground());
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto &w = app.createWindow<Window>({
        .size = {1180, 960},
        .title = "Flux — Semantic Theme Demo",
        .resizable = true,
    });

    w.setView<TypographyDemoRoot>();
    return app.exec();
}
