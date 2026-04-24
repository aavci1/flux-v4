#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/Card.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Slider.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/Toggle.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <memory>
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

Element metricTile(Theme const &theme, std::string value, std::string label, Color accent) {
    return VStack {
        .spacing = theme.space1,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(value),
                .font = Font::title2(),
                .color = accent,
                .horizontalAlignment = HorizontalAlignment::Leading,
            },
            Text {
                .text = std::move(label),
                .font = Font::footnote(),
                .color = Color::secondary(),
                .horizontalAlignment = HorizontalAlignment::Leading,
            }
        )
    }
        .padding(theme.space3)
        .fill(Color::windowBackground())
        .cornerRadius(theme.radiusMedium)
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

        float clamped = std::clamp(*progress, 0.f, 1.f);
        auto previewValue = useState(clamped);
        previewValue.setSilently(clamped);

        return makeSectionCard(
            theme, "Playback Controls",
            "Drive one handle with play, pause, resume, stop, finite repeats, infinite repeats, and delayed starts.",
            VStack {
                .spacing = theme.space3,
                .alignment = Alignment::Stretch,
                .children = children(
                    Slider {
                        .value = previewValue,
                        .min = 0.f,
                        .max = 1.f,
                        .disabled = true
                    },
                    HStack {
                        .spacing = theme.space3,
                        .alignment = Alignment::Stretch,
                        .children = children(
                            metricTile(theme, state, "State", Color::accent()),
                            metricTile(theme, formatFloat(progress), "Progress", Color::success()),
                            metricTile(theme, theme.reducedMotion ? "On" : "Off", "Reduced motion",
                                       theme.reducedMotion ? Color::warning() : Color::secondary())
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
        auto fill = useAnimation<Color>(Color::accent());

        auto calmPreset = [travel, width, height, radius, lift, fill, theme] {
            WithTransition transition {Transition::ease(std::max(0.01f, theme.durationSlow))};
            travel = 0.f;
            width = 132.f;
            height = 52.f;
            radius = theme.radiusLarge;
            lift = 0.f;
            fill = Color::accent();
        };
        auto springPreset = [travel, width, height, radius, lift, fill, theme] {
            WithTransition transition {Transition::spring(420.f, 24.f, 0.70f)};
            travel = 1.f;
            width = 194.f;
            height = 74.f;
            radius = 30.f;
            lift = -10.f;
            fill = Color::warning();
        };
        auto snapPreset = [travel, width, height, radius, lift, fill, theme] {
            travel.set(0.35f, Transition::instant());
            width.set(116.f, Transition::instant());
            height.set(40.f, Transition::instant());
            radius.set(theme.radiusFull, Transition::instant());
            lift.set(10.f, Transition::instant());
            fill.set(Color::success(), Transition::instant());
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
                                .cornerRadius(theme.radiusLarge)
                                .fill(Color::windowBackground())
                                .stroke(Color::separator(), 1.f),
                            Rectangle {}
                                .size(previewWidth - 32.f, 2.f)
                                .cornerRadius(1.f)
                                .fill(Color::separator())
                                .position(16.f, 58.f),
                            Rectangle {}
                                .size(*width, *height)
                                .cornerRadius(*radius)
                                .fill(*fill)
                                .position(objectX, 32.f + *lift)
                        )
                    }
                        .size(previewWidth, 118.f),
                    HStack {
                        .spacing = theme.space3,
                        .alignment = Alignment::Stretch,
                        .children = children(
                            metricTile(theme, formatFloat(*travel), "Travel", Color::accent()),
                            metricTile(theme, formatFloat(*width), "Width", Color::warning()),
                            metricTile(theme, formatFloat(*radius), "Corner radius", Color::success())
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

struct AmbientLoopDriver {
    static constexpr float kPreviewWidth = 260.f;
    static constexpr float kPreviewHeight = 104.f;
    static constexpr float kBarWidth = 22.f;
    static constexpr float kBarBaseHeight = 18.f;
    static constexpr float kBarTravel = 34.f;
    static constexpr float kBarCornerRadius = 11.f;
    static constexpr int kBarCount = 5;
    static constexpr double kLegDurationSeconds = 1.4;

    AmbientLoopDriver()
        : benchmarkEnabled_(std::getenv("FLUX_ANIMATION_DEMO_BENCH") != nullptr)
        , benchmarkWindowStart_(std::chrono::steady_clock::now()) {}

    ~AmbientLoopDriver() {
        stop();
    }

    void syncReducedMotion(bool reducedMotion) {
        if (reducedMotion_ == reducedMotion) {
            return;
        }
        reducedMotion_ = reducedMotion;
        if (reducedMotion_) {
            stop();
        } else {
            start();
        }
        requestRedraw();
    }

    void ensureStarted() {
        if (!reducedMotion_) {
            start();
        }
    }

    [[nodiscard]] float phase() const {
        if (reducedMotion_) {
            return 1.f;
        }
        double const elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
        double cycle = std::fmod(elapsed / kLegDurationSeconds, 2.0);
        if (cycle < 0.0) {
            cycle += 2.0;
        }
        return static_cast<float>(cycle <= 1.0 ? cycle : 2.0 - cycle);
    }

    void recordDraw() {
        if (!benchmarkEnabled_) {
            return;
        }
        ++drawCount_;
        auto const now = std::chrono::steady_clock::now();
        double const elapsed = std::chrono::duration<double>(now - benchmarkWindowStart_).count();
        if (elapsed < 1.0) {
            return;
        }
        std::fprintf(stderr, "[animation-demo bench] ambient-loop fps=%.1f draws=%zu window=%.2fs\n",
                     static_cast<double>(drawCount_) / elapsed, drawCount_, elapsed);
        std::fflush(stderr);
        benchmarkWindowStart_ = now;
        drawCount_ = 0;
    }

private:
    void start() {
        if (tickHandle_.isValid()) {
            return;
        }
        startTime_ = std::chrono::steady_clock::now();
        tickHandle_ = AnimationClock::instance().subscribe([](AnimationTick const&) {
            requestRedraw();
        });
    }

    void stop() {
        if (!tickHandle_.isValid()) {
            return;
        }
        AnimationClock::instance().unsubscribe(tickHandle_);
        tickHandle_ = {};
    }

    static void requestRedraw() {
        if (Application::hasInstance()) {
            Application::instance().requestRedraw();
        }
    }

    bool reducedMotion_ = false;
    bool benchmarkEnabled_ = false;
    ObserverHandle tickHandle_{};
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point benchmarkWindowStart_;
    std::size_t drawCount_ = 0;
};

struct AmbientLoopLab : ViewModifiers<AmbientLoopLab> {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        std::shared_ptr<AmbientLoopDriver> loop =
            useMemo([] { return std::make_shared<AmbientLoopDriver>(); });
        loop->syncReducedMotion(theme.reducedMotion);
        loop->ensureStarted();

        return makeSectionCard(
            theme, "Auto-Running Loop",
            "This panel keeps its decorative loop on the frame clock and redraw path so the rest of the demo does not rebuild on every display-link tick. Reduced motion still settles it on the final frame.",
            VStack {
                .spacing = theme.space3,
                .children = children(
                    Render {
                        .measureFn = [](LayoutConstraints const&, LayoutHints const&) {
                            return Size {AmbientLoopDriver::kPreviewWidth, AmbientLoopDriver::kPreviewHeight};
                        },
                        .draw = [loop, theme](Canvas& canvas, Rect frame) {
                            loop->recordDraw();
                            float const phase = loop->phase();
                            Rect const preview {
                                frame.x,
                                frame.y,
                                AmbientLoopDriver::kPreviewWidth,
                                AmbientLoopDriver::kPreviewHeight,
                            };
                            canvas.drawRect(preview, CornerRadius {theme.radiusLarge},
                                            FillStyle::solid(theme.windowBackgroundColor),
                                            StrokeStyle::solid(theme.separatorColor, 1.f));

                            float const totalWidth =
                                AmbientLoopDriver::kBarWidth * static_cast<float>(AmbientLoopDriver::kBarCount) +
                                theme.space2 * static_cast<float>(AmbientLoopDriver::kBarCount - 1);
                            float const startX = preview.x + (preview.width - totalWidth) * 0.5f;
                            for (int i = 0; i < AmbientLoopDriver::kBarCount; ++i) {
                                float const anchor = static_cast<float>(i) /
                                                     static_cast<float>(AmbientLoopDriver::kBarCount - 1);
                                float const emphasis =
                                    std::clamp(1.f - std::abs(phase - anchor) * 2.8f, 0.f, 1.f);
                                float const barHeight =
                                    AmbientLoopDriver::kBarBaseHeight +
                                    emphasis * AmbientLoopDriver::kBarTravel;
                                Rect const bar {
                                    startX + static_cast<float>(i) *
                                                 (AmbientLoopDriver::kBarWidth + theme.space2),
                                    preview.y + (preview.height - barHeight) * 0.5f,
                                    AmbientLoopDriver::kBarWidth,
                                    barHeight,
                                };
                                canvas.drawRect(bar, CornerRadius {AmbientLoopDriver::kBarCornerRadius},
                                                FillStyle::solid(alpha(theme.accentColor,
                                                                       0.30f + emphasis * 0.70f)),
                                                StrokeStyle::none());
                            }
                        },
                        .pure = false,
                    },
                    HStack {
                        .spacing = theme.space3,
                        .alignment = Alignment::Stretch,
                        .children = children(
                            metricTile(theme, theme.reducedMotion ? "Settled" : "Looping", "Runtime state",
                                       theme.reducedMotion ? Color::warning() : Color::success()),
                            metricTile(theme, "Render", "Update path", Color::accent()),
                            metricTile(theme, theme.reducedMotion ? "On" : "Off", "Reduced motion",
                                       theme.reducedMotion ? Color::warning() : Color::secondary())
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
                            .font = Font::largeTitle(),
                            .color = Color::primary(),
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Text {
                            .text = "Detailed useAnimation walkthrough: explicit play/set controls, repeat and autoreverse, synchronized WithTransition updates, and automatic reduced-motion handling from Theme.",
                            .font = Font::body(),
                            .color = Color::secondary(),
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
                                                .font = Font::headline(),
                                                .color = Color::primary(),
                                                .horizontalAlignment = HorizontalAlignment::Leading,
                                            },
                                            Text {
                                                .text = *reducedMotion
                                                            ? "Transitions collapse to their final values."
                                                            : "Animations run with their configured timing and repeats.",
                                                .font = Font::footnote(),
                                                .color = Color::secondary(),
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
                            .font = demoTheme.footnoteFont,
                            .color = demoTheme.tertiaryLabelColor,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        }
                    )
                }
                    .padding(demoTheme.space5)
            )
        }
            .fill(demoTheme.windowBackgroundColor)
            .environment(demoTheme);
    }
};

} // namespace

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    auto &w = app.createWindow<Window>({
        .size = {800, 800},
        .title = "Flux - Animation demo",
        .resizable = true,
    });
    w.setView<AnimationDemoRoot>();
    return app.exec();
}
