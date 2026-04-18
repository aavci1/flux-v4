#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Slider.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

namespace {

std::string fmtInt(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(v));
    return buf;
}

std::string fmtHex(float r, float g, float b) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", static_cast<int>(r), static_cast<int>(g),
                  static_cast<int>(b));
    return buf;
}

std::string fmtMinutes(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d min", static_cast<int>(v));
    return buf;
}

std::string fmtPercent(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(v));
    return buf;
}

Element makeSectionCard(Theme const &theme, std::string title, std::string caption, Element content) {
    return VStack {
        .spacing = theme.space3,
        .children = children(
            Text {.text = std::move(title), .font = theme.fontTitle, .color = theme.colorTextPrimary},
            Text {
                .text = std::move(caption),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .wrapping = TextWrapping::Wrap,
            },
            std::move(content)
        )
    } //
        .padding(theme.space4)
        .fill(FillStyle::solid(theme.colorSurfaceOverlay))
        .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
        .cornerRadius(CornerRadius {theme.radiusLarge});
}

Element labeledSlider(Theme const &theme, std::string label, std::string valueText, State<float> value,
                      float min, float max, float step, Slider::Style style = {}) {
    return VStack {
        .spacing = theme.space2,
        .children = children(
            HStack {
                .spacing = theme.space2,
                .alignment = Alignment::Center,
                .children = children(
                    Text {.text = std::move(label), .font = theme.fontLabel, .color = theme.colorTextPrimary},
                    Spacer {},
                    Text {.text = std::move(valueText), .font = theme.fontLabel, .color = theme.colorTextSecondary}
                )
            },
            Slider {
                .value = value,
                .min = min,
                .max = max,
                .step = step,
                .style = style,
            }
        )
    } //
        .padding(theme.space3)
        .fill(FillStyle::solid(theme.colorBackground))
        .cornerRadius(CornerRadius {theme.radiusMedium});
}

float luminance(Color const &c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

constexpr float kChannelScale = 1.f / 255.f;

} // namespace

struct SliderDemoRoot {
    Element body() const {
        Theme const &theme = useEnvironment<Theme>();

        auto red = useState(90.f);
        auto green = useState(120.f);
        auto blue = useState(200.f);
        auto volume = useState(68.f);
        auto scrubber = useState(42.f);

        Color const preview = Color {*red * kChannelScale, *green * kChannelScale, *blue * kChannelScale, 1.f};

        Element page = Element {VStack {
                                    .spacing = theme.space4,
                                    .children = children(
                                        Text {
                                            .text = "Slider Demo",
                                            .font = theme.fontDisplay,
                                            .color = theme.colorTextPrimary,
                                            .horizontalAlignment = HorizontalAlignment::Leading,
                                        },
                                        Text {
                                            .text = "Three focused examples: color mixing, media-style scrubbing, and compact percentage tuning.",
                                            .font = theme.fontBody,
                                            .color = theme.colorTextSecondary,
                                            .horizontalAlignment = HorizontalAlignment::Leading,
                                            .wrapping = TextWrapping::Wrap,
                                        },
                                        makeSectionCard(
                                            theme, "RGB Mixer",
                                            "A slider should feel at home in a composed panel, not just as a lone horizontal line.",
                                            VStack {
                                                .spacing = theme.space3,
                                                .children = children(
                                                    ZStack {
                                                        .horizontalAlignment = Alignment::Center,
                                                        .verticalAlignment = Alignment::Center,
                                                        .children = children(
                                                            Rectangle {}
                                                                .fill(FillStyle::solid(preview))
                                                                .stroke(StrokeStyle::solid(theme.colorBorder, 1.f))
                                                                .height(150.f)
                                                                .cornerRadius(CornerRadius {theme.radiusLarge})
                                                                .flex(1.f, 1.f, 0.f),
                                                            Text {
                                                                .text = fmtHex(*red, *green, *blue),
                                                                .font = theme.fontTitle,
                                                                .color = luminance(preview) > 0.55f ? theme.colorTextPrimary : theme.colorOnAccent,
                                                                .horizontalAlignment = HorizontalAlignment::Center,
                                                                .verticalAlignment = VerticalAlignment::Center,
                                                            }
                                                        )
                                                    },
                                                    labeledSlider(theme, "Red", fmtInt(*red), red, 0.f, 255.f, 1.f,
                                                                  Slider::Style {.activeColor = theme.colorDanger}),
                                                    labeledSlider(theme, "Green", fmtInt(*green), green, 0.f, 255.f, 1.f,
                                                                  Slider::Style {.activeColor = theme.colorSuccess}),
                                                    labeledSlider(theme, "Blue", fmtInt(*blue), blue, 0.f, 255.f, 1.f,
                                                                  Slider::Style {.activeColor = theme.colorAccent})
                                                )
                                            }
                                        ),
                                        makeSectionCard(
                                            theme, "Media Scrubber",
                                            "The same primitive can read as a playback control when the surrounding layout explains what the value means.",
                                            VStack {
                                                .spacing = theme.space3,
                                                .children = children(
                                                    HStack {
                                                        .spacing = theme.space2,
                                                        .alignment = Alignment::Center,
                                                        .children = children(
                                                            Text {.text = "Playback position", .font = theme.fontLabel, .color = theme.colorTextPrimary},
                                                            Spacer {},
                                                            Text {.text = fmtMinutes(*scrubber), .font = theme.fontLabel, .color = theme.colorTextSecondary}
                                                        )
                                                    },
                                                    Slider {
                                                        .value = scrubber,
                                                        .min = 0.f,
                                                        .max = 96.f,
                                                        .step = 1.f,
                                                        .style = Slider::Style {
                                                            .activeColor = theme.colorAccent,
                                                            .trackHeight = 6.f,
                                                            .thumbSize = 18.f,
                                                        },
                                                    },
                                                    Text {
                                                        .text = "Arrow keys nudge the value; hold Shift for larger jumps.",
                                                        .font = theme.fontBodySmall,
                                                        .color = theme.colorTextSecondary,
                                                        .wrapping = TextWrapping::Wrap,
                                                    }
                                                )
                                            }
                                        ),
                                        makeSectionCard(theme, "Compact Controls", "Sliders also work well in shorter utility rows when the value is the main feedback.", VStack {.spacing = theme.space2, .children = children(labeledSlider(theme, "Volume", fmtPercent(*volume), volume, 0.f, 100.f, 1.f, Slider::Style {
                                                                                                                                                                                                                                                                                                                 .activeColor = theme.colorSuccess,
                                                                                                                                                                                                                                                                                                                 .trackHeight = 4.f,
                                                                                                                                                                                                                                                                                                                 .thumbSize = 16.f,
                                                                                                                                                                                                                                                                                                             }),
                                                                                                                                                                                                                                Text {
                                                                                                                                                                                                                                    .text = "The minimum-value fill workaround stays in place here to avoid the zero-width rendering bug you called out.",
                                                                                                                                                                                                                                    .font = theme.fontBodySmall,
                                                                                                                                                                                                                                    .color = theme.colorTextMuted,
                                                                                                                                                                                                                                    .wrapping = TextWrapping::Wrap,
                                                                                                                                                                                                                                })})
                                    )
                                }}
                           .padding(theme.space5);

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(std::move(page)),
        }
            .fill(FillStyle::solid(theme.colorBackground));
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    auto &w = app.createWindow<Window>({
        .size = {760, 820},
        .title = "Flux - Slider demo",
        .resizable = true,
    });
    w.setView<SliderDemoRoot>();
    return app.exec();
}
