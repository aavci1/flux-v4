#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/Toggle.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace flux;

namespace {

std::string formatFloat(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

Color alpha(Color color, float opacity) {
    return Color {color.r, color.g, color.b, opacity};
}

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
                .wrapping = TextWrapping::Wrap,
            },
            std::move(content)
        )
    }
        .padding(theme.space4)
        .fill(FillStyle::solid(theme.colorSurfaceOverlay))
        .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
        .cornerRadius(CornerRadius {theme.radiusLarge});
}

Element metricTile(Theme const &theme, std::string value, std::string label, Color accent) {
    return VStack {
        .spacing = theme.space1,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(value),
                .font = theme.fontTitle,
                .color = accent,
                .horizontalAlignment = HorizontalAlignment::Leading,
            },
            Text {
                .text = std::move(label),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .horizontalAlignment = HorizontalAlignment::Leading,
            }
        )
    }
        .padding(theme.space3)
        .fill(FillStyle::solid(theme.colorBackground))
        .cornerRadius(CornerRadius {theme.radiusMedium})
        .flex(1.f, 1.f, 0.f);
}

Element buttonRow(Theme const &theme, std::vector<Element> buttons) {
    return HStack {
        .spacing = theme.space2,
        .alignment = Alignment::Center,
        .children = std::move(buttons),
    };
}

struct PlaybackLab : ViewModifiers<PlaybackLab> {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        auto progress = useAnimation<float>(0.f);

        float const clamped = std::clamp(*progress, 0.f, 1.f);
        float const railWidth = 260.f;
        float const knobSize = 28.f;
        float const usableWidth = railWidth - knobSize;
        float const knobX = usableWidth * clamped;
        std::string const state = progress.isPaused() ? "Paused" : progress.isRunning() ? "Running" : "Idle";

        auto playOnce = [progress, theme] {
            progress.set(0.f, Transition::instant());
            progress.play(1.f, Transition::ease(std::max(0.01f, theme.durationSlow)).delayed(0.12f));
        };
        auto loopForever = [progress] {
            progress.set(0.f, Transition::instant());
            progress.play(1.f, AnimationOptions {
                                    .transition = Transition::linear(1.2f),
                                    .repeat = AnimationOptions::kRepeatForever,
                                    .autoreverse = true,
                                });
        };
        auto bounceFour = [progress] {
            progress.set(0.f, Transition::instant());
            progress.play(1.f, AnimationOptions {
                                    .transition = Transition::ease(0.28f),
                                    .repeat = 4,
                                    .autoreverse = true,
                                });
        };
        auto pause = [progress] { progress.pause(); };
        auto resume = [progress] { progress.resume(); };
        auto stop = [progress] { progress.stop(); };
        auto reset = [progress] {
            progress.stop();
            progress.set(0.f, Transition::instant());
        };

        return makeSectionCard(
            theme, "Playback Controls",
            "Drive one handle with play, pause, resume, stop, finite repeats, infinite repeats, and delayed starts.",
            VStack {
                .spacing = theme.space3,
                .children = children(
                    ZStack {
                        .horizontalAlignment = Alignment::Start,
                        .verticalAlignment = Alignment::Start,
                        .children = children(
                            Rectangle {}
                                .size(railWidth, 86.f)
                                .cornerRadius(CornerRadius {theme.radiusLarge})
                                .fill(FillStyle::solid(theme.colorBackground))
                                .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f)),
                            Rectangle {}
                                .size(railWidth - 40.f, 8.f)
                                .cornerRadius(CornerRadius {4.f})
                                .fill(FillStyle::solid(theme.colorSurfaceDisabled))
                                .position(20.f, 39.f),
                            Rectangle {}
                                .size(std::max(14.f, knobX + knobSize * 0.5f), 8.f)
                                .cornerRadius(CornerRadius {4.f})
                                .fill(FillStyle::solid(alpha(theme.colorAccent, 0.28f)))
                                .position(20.f, 39.f),
                            Rectangle {}
                                .size(knobSize, knobSize)
                                .cornerRadius(CornerRadius {knobSize * 0.5f})
                                .fill(FillStyle::solid(theme.colorAccent))
                                .stroke(StrokeStyle::solid(Color::hex(0xFFFFFF), 1.f))
                                .position(knobX, 29.f)
                        )
                    }
                        .size(railWidth, 86.f),
                    HStack {
                        .spacing = theme.space3,
                        .alignment = Alignment::Stretch,
                        .children = children(
                            metricTile(theme, state, "State", theme.colorAccent),
                            metricTile(theme, formatFloat(clamped), "Progress", theme.colorSuccess),
                            metricTile(theme, theme.reducedMotion ? "On" : "Off", "Reduced motion",
                                       theme.reducedMotion ? theme.colorWarning : theme.colorTextSecondary)
                        )
                    },
                    buttonRow(
                        theme,
                        std::vector<Element> {
                            Button {.label = "Play Once", .variant = ButtonVariant::Primary, .onTap = playOnce},
                            Button {.label = "Loop Forever", .variant = ButtonVariant::Secondary, .onTap = loopForever},
                            Button {.label = "Bounce 4x", .variant = ButtonVariant::Secondary, .onTap = bounceFour},
                        }
                    ),
                    buttonRow(
                        theme,
                        std::vector<Element> {
                            Button {.label = "Pause", .variant = ButtonVariant::Ghost, .disabled = !progress.isRunning() || progress.isPaused(), .onTap = pause},
                            Button {.label = "Resume", .variant = ButtonVariant::Ghost, .disabled = !progress.isPaused(), .onTap = resume},
                            Button {.label = "Stop", .variant = ButtonVariant::Ghost, .disabled = !progress.isRunning() && !progress.isPaused(), .onTap = stop},
                            Button {.label = "Reset", .variant = ButtonVariant::Ghost, .onTap = reset},
                        }
                    )
                )
            }
        );
    }
};

struct MorphLab : ViewModifiers<MorphLab> {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        auto travel = useAnimation<float>(0.f);
        auto width = useAnimation<float>(132.f);
        auto height = useAnimation<float>(52.f);
        auto radius = useAnimation<float>(16.f);
        auto lift = useAnimation<float>(0.f);
        auto fill = useAnimation<Color>(theme.colorAccent);

        auto calmPreset = [travel, width, height, radius, lift, fill, theme] {
            WithTransition transition {Transition::ease(std::max(0.01f, theme.durationSlow))};
            travel = 0.f;
            width = 132.f;
            height = 52.f;
            radius = theme.radiusLarge;
            lift = 0.f;
            fill = theme.colorAccent;
        };
        auto springPreset = [travel, width, height, radius, lift, fill, theme] {
            WithTransition transition {Transition::spring(420.f, 24.f, 0.70f)};
            travel = 1.f;
            width = 194.f;
            height = 74.f;
            radius = 30.f;
            lift = -10.f;
            fill = theme.colorWarning;
        };
        auto snapPreset = [travel, width, height, radius, lift, fill, theme] {
            travel.set(0.35f, Transition::instant());
            width.set(116.f, Transition::instant());
            height.set(40.f, Transition::instant());
            radius.set(theme.radiusFull, Transition::instant());
            lift.set(10.f, Transition::instant());
            fill.set(theme.colorSuccess, Transition::instant());
        };

        float const previewWidth = 260.f;
        float const objectX = 18.f + (previewWidth - *width - 36.f) * std::clamp(*travel, 0.f, 1.f);

        return makeSectionCard(
            theme, "WithTransition Scope",
            "A single WithTransition scope lets several useAnimation handles share one easing or spring without repeating the transition at each call site.",
            VStack {
                .spacing = theme.space3,
                .children = children(
                    ZStack {
                        .horizontalAlignment = Alignment::Start,
                        .verticalAlignment = Alignment::Start,
                        .children = children(
                            Rectangle {}
                                .size(previewWidth, 118.f)
                                .cornerRadius(CornerRadius {theme.radiusLarge})
                                .fill(FillStyle::solid(theme.colorBackground))
                                .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f)),
                            Rectangle {}
                                .size(previewWidth - 32.f, 2.f)
                                .cornerRadius(CornerRadius {1.f})
                                .fill(FillStyle::solid(theme.colorBorderSubtle))
                                .position(16.f, 58.f),
                            Rectangle {}
                                .size(*width, *height)
                                .cornerRadius(CornerRadius {*radius})
                                .fill(FillStyle::solid(*fill))
                                .position(objectX, 32.f + *lift)
                        )
                    }
                        .size(previewWidth, 118.f),
                    HStack {
                        .spacing = theme.space3,
                        .alignment = Alignment::Stretch,
                        .children = children(
                            metricTile(theme, formatFloat(*travel), "Travel", theme.colorAccent),
                            metricTile(theme, formatFloat(*width), "Width", theme.colorWarning),
                            metricTile(theme, formatFloat(*radius), "Corner radius", theme.colorSuccess)
                        )
                    },
                    buttonRow(
                        theme,
                        std::vector<Element> {
                            Button {.label = "Calm Ease", .variant = ButtonVariant::Primary, .onTap = calmPreset},
                            Button {.label = "Spring Burst", .variant = ButtonVariant::Secondary, .onTap = springPreset},
                            Button {.label = "Snap Chip", .variant = ButtonVariant::Ghost, .onTap = snapPreset},
                        }
                    )
                )
            }
        );
    }
};

struct AmbientLoopLab : ViewModifiers<AmbientLoopLab> {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        auto phase = useAnimation<float>(
            0.f,
            AnimationOptions {
                .transition = Transition::linear(1.4f),
                .repeat = AnimationOptions::kRepeatForever,
                .autoreverse = true,
            }
        );
        if (theme.reducedMotion) {
            if (phase.isRunning() || std::abs(*phase - 1.f) > 0.001f) {
                phase.set(1.f, Transition::instant());
            }
        } else if (!phase.isRunning()) {
            if (*phase >= 0.999f) {
                phase.set(0.f, Transition::instant());
            }
            phase.play(1.f);
        }

        std::vector<Element> bars;
        bars.reserve(5);
        for (int i = 0; i < 5; ++i) {
            float const anchor = static_cast<float>(i) / 4.f;
            float const emphasis = std::clamp(1.f - std::abs(*phase - anchor) * 2.8f, 0.f, 1.f);
            float const barHeight = 18.f + emphasis * 34.f;
            bars.push_back(
                Rectangle {}
                    .size(22.f, barHeight)
                    .cornerRadius(CornerRadius {11.f})
                    .fill(FillStyle::solid(alpha(theme.colorAccent, 0.30f + emphasis * 0.70f)))
            );
        }

        return makeSectionCard(
            theme, "Auto-Running Loop",
            "This panel starts a repeating autoreversing animation from body(). When reduced motion is turned on, the loop collapses to its final frame and resumes when motion is re-enabled.",
            VStack {
                .spacing = theme.space3,
                .children = children(
                    ZStack {
                        .horizontalAlignment = Alignment::Center,
                        .verticalAlignment = Alignment::Center,
                        .children = children(
                            Rectangle {}
                                .size(260.f, 104.f)
                                .cornerRadius(CornerRadius {theme.radiusLarge})
                                .fill(FillStyle::solid(theme.colorBackground))
                                .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f)),
                            HStack {
                                .spacing = theme.space2,
                                .alignment = Alignment::Center,
                                .children = std::move(bars),
                            }
                        )
                    }
,
                    HStack {
                        .spacing = theme.space3,
                        .alignment = Alignment::Stretch,
                        .children = children(
                            metricTile(theme, formatFloat(*phase), "Phase", theme.colorAccent),
                            metricTile(theme, phase.isRunning() ? "Looping" : "Settled", "Runtime state",
                                       phase.isRunning() ? theme.colorSuccess : theme.colorWarning)
                        )
                    }
                )
            }
        );
    }
};

struct AnimationDemoRoot {
    auto body() const {
        Theme const &windowTheme = useEnvironment<Theme>();
        auto reducedMotion = useState(false);

        Theme demoTheme = windowTheme;
        demoTheme.reducedMotion = *reducedMotion;

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = demoTheme.space4,
                    .alignment = Alignment::Start,
                    .children = children(
                        Text {
                            .text = "Animation Demo",
                            .font = demoTheme.fontDisplay,
                            .color = demoTheme.colorTextPrimary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Text {
                            .text = "Detailed useAnimation walkthrough: explicit play/set controls, repeat and autoreverse, synchronized WithTransition updates, and automatic reduced-motion handling from Theme.",
                            .font = demoTheme.fontBody,
                            .color = demoTheme.colorTextSecondary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        },
                        makeSectionCard(
                            demoTheme, "Environment",
                            "The toggle below flips Theme::reducedMotion for the whole demo subtree. Hook-backed animations snap instantly when it is enabled.",
                            HStack {
                                .spacing = demoTheme.space3,
                                .alignment = Alignment::Center,
                                .children = children(
                                    VStack {
                                        .spacing = demoTheme.space1,
                                        .alignment = Alignment::Start,
                                        .children = children(
                                            Text {
                                                .text = "Reduced motion",
                                                .font = demoTheme.fontLabel,
                                                .color = demoTheme.colorTextPrimary,
                                                .horizontalAlignment = HorizontalAlignment::Leading,
                                            },
                                            Text {
                                                .text = *reducedMotion
                                                            ? "Transitions collapse to their final values."
                                                            : "Animations run with their configured timing and repeats.",
                                                .font = demoTheme.fontBodySmall,
                                                .color = demoTheme.colorTextSecondary,
                                                .horizontalAlignment = HorizontalAlignment::Leading,
                                                .wrapping = TextWrapping::Wrap,
                                            }
                                        )
                                    }
                                        .flex(1.f, 1.f, 0.f),
                                    Toggle {.value = reducedMotion}
                                )
                            }
                        ),
                        PlaybackLab {},
                        MorphLab {},
                        AmbientLoopLab {},
                        Text {
                            .text = "Tip: the playback panel demonstrates imperative control; the morph panel demonstrates scoped transitions; the ambient panel demonstrates a loop owned by the component body.",
                            .font = demoTheme.fontBodySmall,
                            .color = demoTheme.colorTextMuted,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        }
                    )
                }
                    .padding(demoTheme.space5)
            )
        }
            .fill(FillStyle::solid(demoTheme.colorBackground))
            .environment(demoTheme);
    }
};

} // namespace

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    auto &w = app.createWindow<Window>({
        .size = {900, 900},
        .title = "Flux - Animation demo",
        .resizable = true,
    });
    w.setView<AnimationDemoRoot>();
    return app.exec();
}
