#include <Flux/Core/Application.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/SceneGraph/TextNode.hpp>
#include <Flux/UI/Theme.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace flux;
using namespace flux::scenegraph;

constexpr Size kWindowSize {840.f, 760.f};

struct BuiltNode {
    std::unique_ptr<SceneNode> node;
    Size size {};
};

struct TextMeasure {
    std::shared_ptr<TextLayout const> layout;
    Size size {};
};

struct AnimatedThumb {
    RectNode *thumb = nullptr;
    float minX = 0.f;
    float maxX = 0.f;
    float y = 0.f;
    float phase = 0.f;
    float speed = 1.f;
};

struct ToggleVisual {
    float trackWidth = 44.f;
    float trackHeight = 26.f;
    float thumbInset = 4.f;
    Color onColor = Colors::blue;
    float phase = 0.f;
    float speed = 1.f;
};

struct SceneBuild {
    std::unique_ptr<SceneNode> root;
    GroupNode *content = nullptr;
    std::vector<AnimatedThumb> thumbs;
    float maxScrollOffset = 0.f;
};

Color withAlpha(Color color, float alpha) {
    color.a = alpha;
    return color;
}

float oscillate(float timeSeconds, float phase, float speed) {
    return 0.5f + 0.5f * std::sin(timeSeconds * speed + phase);
}

TextMeasure layoutText(TextSystem &textSystem, std::string_view text, Font const &font, Color const &color,
                       float maxWidth) {
    TextLayoutOptions const options {};
    Size const measuredSize = textSystem.measure(text, font, color, maxWidth, options);
    auto layout = textSystem.layout(
        text,
        font,
        color,
        Rect {0.f, 0.f, maxWidth > 0.f ? maxWidth : measuredSize.width, measuredSize.height},
        options
    );
    return {
        .layout = std::move(layout),
        .size = measuredSize,
    };
}

TextNode *appendTextNode(SceneNode &parent, Point position, TextMeasure text) {
    auto node = std::make_unique<TextNode>(
        Rect {position.x, position.y, text.size.width, text.size.height},
        std::move(text.layout)
    );
    TextNode *raw = node.get();
    parent.appendChild(std::move(node));
    return raw;
}

RectNode *appendRectNode(SceneNode &parent, Rect bounds, FillStyle fill = FillStyle::none(),
                         StrokeStyle stroke = StrokeStyle::none(), CornerRadius cornerRadius = {},
                         ShadowStyle shadow = ShadowStyle::none()) {
    auto node = std::make_unique<RectNode>(bounds, fill, stroke, cornerRadius, shadow);
    RectNode *raw = node.get();
    parent.appendChild(std::move(node));
    return raw;
}

BuiltNode buildToggle(Theme const &theme, ToggleVisual visual, std::vector<AnimatedThumb> &animatedThumbs) {
    auto toggle = std::make_unique<GroupNode>(Rect {0.f, 0.f, visual.trackWidth, visual.trackHeight});

    appendRectNode(
        *toggle,
        Rect {0.f, 0.f, visual.trackWidth, visual.trackHeight},
        FillStyle::solid(withAlpha(visual.onColor, 0.28f)),
        StrokeStyle::solid(withAlpha(visual.onColor, 0.52f), 1.f),
        CornerRadius {visual.trackHeight * 0.5f}
    );

    float const thumbSize = std::max(visual.trackHeight - 2.f * visual.thumbInset, 0.f);
    float const minX = visual.thumbInset;
    float const maxX = visual.trackWidth - visual.thumbInset - thumbSize;
    float const thumbX = minX + (maxX - minX) * oscillate(0.f, visual.phase, visual.speed);

    RectNode *thumb = appendRectNode(
        *toggle,
        Rect {thumbX, visual.thumbInset, thumbSize, thumbSize},
        FillStyle::solid(theme.toggleThumbColor),
        StrokeStyle::none(),
        CornerRadius {thumbSize * 0.5f},
        ShadowStyle {
            .radius = theme.shadowRadiusControl,
            .offset = {0.f, theme.shadowOffsetYControl},
            .color = theme.shadowColor,
        }
    );

    animatedThumbs.push_back(AnimatedThumb {
        .thumb = thumb,
        .minX = minX,
        .maxX = maxX,
        .y = visual.thumbInset,
        .phase = visual.phase,
        .speed = visual.speed,
    });

    return {
        .node = std::move(toggle),
        .size = Size {visual.trackWidth, visual.trackHeight},
    };
}

BuiltNode buildSettingRow(TextSystem &textSystem, Theme const &theme, float width, std::string_view title,
                          std::string_view detail, ToggleVisual toggleVisual,
                          std::vector<AnimatedThumb> &animatedThumbs) {
    float const padding = theme.space3;
    float const spacing = theme.space3;

    BuiltNode toggle = buildToggle(theme, toggleVisual, animatedThumbs);
    float const textWidth = std::max(width - padding * 2.f - spacing - toggle.size.width, 0.f);

    TextMeasure titleText = layoutText(textSystem, title, theme.headlineFont, theme.labelColor, textWidth);
    TextMeasure detailText = layoutText(textSystem, detail, theme.calloutFont, theme.secondaryLabelColor, textWidth);

    float const textStackHeight = titleText.size.height + theme.space1 + detailText.size.height;
    float const contentHeight = std::max(textStackHeight, toggle.size.height);
    float const height = padding * 2.f + contentHeight;

    auto row = std::make_unique<RectNode>(
        Rect {0.f, 0.f, width, height},
        FillStyle::solid(theme.windowBackgroundColor),
        StrokeStyle::none(),
        CornerRadius {theme.radiusMedium}
    );

    float const textTop = padding + (contentHeight - textStackHeight) * 0.5f;
    float const detailY = textTop + titleText.size.height + theme.space1;
    appendTextNode(*row, Point {padding, textTop}, std::move(titleText));
    appendTextNode(*row, Point {padding, detailY}, std::move(detailText));

    toggle.node->setPosition(Point {
        width - padding - toggle.size.width,
        padding + (contentHeight - toggle.size.height) * 0.5f,
    });
    row->appendChild(std::move(toggle.node));

    return {
        .node = std::move(row),
        .size = Size {width, height},
    };
}

BuiltNode buildMetricTile(TextSystem &textSystem, Theme const &theme, float width, std::string_view value,
                          std::string_view label, Color accent) {
    float const padding = theme.space3;
    float const maxWidth = std::max(width - padding * 2.f, 0.f);

    TextMeasure valueText = layoutText(textSystem, value, theme.title2Font, accent, maxWidth);
    TextMeasure labelText = layoutText(textSystem, label, theme.footnoteFont, theme.secondaryLabelColor, maxWidth);

    float const height = padding * 2.f + valueText.size.height + theme.space1 + labelText.size.height;

    auto tile = std::make_unique<RectNode>(
        Rect {0.f, 0.f, width, height},
        FillStyle::solid(theme.windowBackgroundColor),
        StrokeStyle::none(),
        CornerRadius {theme.radiusMedium}
    );

    appendTextNode(*tile, Point {padding, padding}, std::move(valueText));
    appendTextNode(*tile, Point {padding, padding + valueText.size.height + theme.space1}, std::move(labelText));

    return {
        .node = std::move(tile),
        .size = Size {width, height},
    };
}

BuiltNode buildMetricsRow(TextSystem &textSystem, Theme const &theme, float width) {
    float const spacing = theme.space3;
    float const tileWidth = std::max((width - spacing * 2.f) / 3.f, 0.f);

    BuiltNode frames = buildMetricTile(textSystem, theme, tileWidth, "60", "Target FPS", theme.accentColor);
    BuiltNode cache = buildMetricTile(textSystem, theme, tileWidth, "Cached", "Prepared ops", theme.successColor);
    BuiltNode viewport = buildMetricTile(textSystem, theme, tileWidth, "Clip", "Viewport scroll", theme.warningColor);

    float const height = std::max(frames.size.height, std::max(cache.size.height, viewport.size.height));
    auto row = std::make_unique<GroupNode>(Rect {0.f, 0.f, width, height});

    frames.node->setPosition(Point {0.f, 0.f});
    cache.node->setPosition(Point {tileWidth + spacing, 0.f});
    viewport.node->setPosition(Point {(tileWidth + spacing) * 2.f, 0.f});

    row->appendChild(std::move(frames.node));
    row->appendChild(std::move(cache.node));
    row->appendChild(std::move(viewport.node));

    return {
        .node = std::move(row),
        .size = Size {width, height},
    };
}

BuiltNode buildSectionCard(TextSystem &textSystem, Theme const &theme, float width, std::string_view title,
                           std::string_view caption, std::vector<BuiltNode> contentNodes,
                           float contentSpacing) {
    float const padding = theme.space4;
    float const maxWidth = std::max(width - padding * 2.f, 0.f);

    TextMeasure titleText = layoutText(textSystem, title, theme.title2Font, theme.labelColor, maxWidth);
    TextMeasure captionText = layoutText(textSystem, caption, theme.bodyFont, theme.secondaryLabelColor, maxWidth);

    float contentHeight = 0.f;
    for (std::size_t i = 0; i < contentNodes.size(); ++i) {
        contentHeight += contentNodes[i].size.height;
        if (i + 1 < contentNodes.size()) {
            contentHeight += contentSpacing;
        }
    }

    float const bodyTop = padding + titleText.size.height + theme.space3 + captionText.size.height;
    float const height = bodyTop + (contentNodes.empty() ? 0.f : theme.space3 + contentHeight) + padding;

    auto card = std::make_unique<RectNode>(
        Rect {0.f, 0.f, width, height},
        FillStyle::solid(theme.elevatedBackgroundColor),
        StrokeStyle::solid(theme.separatorColor, 1.f),
        CornerRadius {theme.radiusLarge},
        ShadowStyle {
            .radius = theme.shadowRadiusPopover,
            .offset = {0.f, theme.shadowOffsetYPopover},
            .color = theme.shadowColor,
        }
    );

    appendTextNode(*card, Point {padding, padding}, std::move(titleText));
    appendTextNode(
        *card,
        Point {padding, padding + titleText.size.height + theme.space3},
        std::move(captionText)
    );

    float cursorY = bodyTop;
    if (!contentNodes.empty()) {
        cursorY += theme.space3;
    }
    for (BuiltNode &content : contentNodes) {
        content.node->setPosition(Point {padding, cursorY});
        card->appendChild(std::move(content.node));
        cursorY += content.size.height + contentSpacing;
    }

    return {
        .node = std::move(card),
        .size = Size {width, height},
    };
}

SceneBuild buildAnimationProbeScene(TextSystem &textSystem, Theme const &theme) {
    float const viewportWidth = kWindowSize.width;
    float const viewportHeight = kWindowSize.height;
    float const outerPadding = theme.space5;
    float const contentWidth = viewportWidth - outerPadding * 2.f;

    SceneBuild build;
    auto root = std::make_unique<GroupNode>(Rect {0.f, 0.f, viewportWidth, viewportHeight});
    auto viewport = std::make_unique<RectNode>(Rect {0.f, 0.f, viewportWidth, viewportHeight});
    viewport->setClipsContents(true);

    auto content = std::make_unique<GroupNode>(Rect {0.f, 0.f, viewportWidth, viewportHeight});
    build.content = content.get();

    TextMeasure heroTitle = layoutText(
        textSystem,
        "Scene Graph Animation Probe",
        theme.largeTitleFont,
        theme.labelColor,
        contentWidth
    );
    TextMeasure heroCaption = layoutText(
        textSystem,
        "Continuous transform-only motion for the retained scene graph: clipped scrolling content plus animated toggle thumbs.",
        theme.bodyFont,
        theme.secondaryLabelColor,
        contentWidth
    );

    TextMeasure note = layoutText(
        textSystem,
        "Use this window for steady-state CPU measurements while the frame pump stays active at 60 FPS.",
        theme.footnoteFont,
        theme.tertiaryLabelColor,
        contentWidth
    );

    std::vector<BuiltNode> metricsContent;
    metricsContent.push_back(buildMetricsRow(textSystem, theme, contentWidth - theme.space4 * 2.f));
    BuiltNode metricsCard = buildSectionCard(
        textSystem,
        theme,
        contentWidth,
        "Animation profile",
        "The scene stays retained. Only node transforms change every tick.",
        std::move(metricsContent),
        theme.space2
    );

    std::vector<BuiltNode> primaryRows;
    primaryRows.push_back(buildSettingRow(
        textSystem,
        theme,
        contentWidth - theme.space4 * 2.f,
        "Live collaboration",
        "Keep presence indicators, cursor signals, and sync affordances moving in a real workspace.",
        ToggleVisual {.onColor = theme.toggleOnColor, .phase = 0.1f, .speed = 1.0f},
        build.thumbs
    ));
    primaryRows.push_back(buildSettingRow(
        textSystem,
        theme,
        contentWidth - theme.space4 * 2.f,
        "Preview animations",
        "Exercise transform-only updates that should reuse prepared render ops instead of rebuilding them.",
        ToggleVisual {.onColor = theme.successColor, .phase = 1.4f, .speed = 1.2f},
        build.thumbs
    ));
    primaryRows.push_back(buildSettingRow(
        textSystem,
        theme,
        contentWidth - theme.space4 * 2.f,
        "Viewport clipping",
        "The entire content column drifts inside a clipped root viewport while leaf nodes stay cached.",
        ToggleVisual {.onColor = theme.warningColor, .phase = 2.2f, .speed = 0.85f},
        build.thumbs
    ));

    BuiltNode primaryCard = buildSectionCard(
        textSystem,
        theme,
        contentWidth,
        "Animated toggles",
        "Each switch thumb moves independently with a phase offset so the renderer cannot coast on a single identical frame.",
        std::move(primaryRows),
        theme.space2
    );

    std::vector<BuiltNode> secondaryRows;
    secondaryRows.push_back(buildSettingRow(
        textSystem,
        theme,
        contentWidth - theme.space4 * 2.f,
        "Background sync",
        "This row mirrors the denser settings surfaces we expect from the element tree later on.",
        ToggleVisual {
            .trackWidth = 34.f,
            .trackHeight = 20.f,
            .thumbInset = 2.f,
            .onColor = theme.toggleOnColor,
            .phase = 0.7f,
            .speed = 1.45f,
        },
        build.thumbs
    ));
    secondaryRows.push_back(buildSettingRow(
        textSystem,
        theme,
        contentWidth - theme.space4 * 2.f,
        "Push delivery",
        "Compact controls still animate through node-local transforms only.",
        ToggleVisual {
            .trackWidth = 34.f,
            .trackHeight = 20.f,
            .thumbInset = 2.f,
            .onColor = theme.successColor,
            .phase = 2.8f,
            .speed = 1.75f,
        },
        build.thumbs
    ));
    secondaryRows.push_back(buildSettingRow(
        textSystem,
        theme,
        contentWidth - theme.space4 * 2.f,
        "Reduced chrome",
        "The demo is visually simple on purpose so the CPU number mostly reflects the framework overhead.",
        ToggleVisual {.onColor = theme.warningColor, .phase = 3.3f, .speed = 1.1f},
        build.thumbs
    ));

    BuiltNode secondaryCard = buildSectionCard(
        textSystem,
        theme,
        contentWidth,
        "Animation notes",
        "This scene is meant to stay continuously active for profiling with Instruments, Activity Monitor, or `ps`.",
        std::move(secondaryRows),
        theme.space2
    );

    BuiltNode footerCard = buildSectionCard(
        textSystem,
        theme,
        contentWidth,
        "What to watch",
        "If CPU stays high here, the remaining floor is likely the present path and frame scheduling rather than scenegraph preparation.",
        {},
        0.f
    );

    float cursorY = outerPadding;
    appendTextNode(*content, Point {outerPadding, cursorY}, std::move(heroTitle));
    cursorY += heroTitle.size.height + theme.space3;

    appendTextNode(*content, Point {outerPadding, cursorY}, std::move(heroCaption));
    cursorY += heroCaption.size.height + theme.space2;

    appendTextNode(*content, Point {outerPadding, cursorY}, std::move(note));
    cursorY += note.size.height + theme.space4;

    metricsCard.node->setPosition(Point {outerPadding, cursorY});
    content->appendChild(std::move(metricsCard.node));
    cursorY += metricsCard.size.height + theme.space4;

    primaryCard.node->setPosition(Point {outerPadding, cursorY});
    content->appendChild(std::move(primaryCard.node));
    cursorY += primaryCard.size.height + theme.space4;

    secondaryCard.node->setPosition(Point {outerPadding, cursorY});
    content->appendChild(std::move(secondaryCard.node));
    cursorY += secondaryCard.size.height + theme.space4;

    footerCard.node->setPosition(Point {outerPadding, cursorY});
    content->appendChild(std::move(footerCard.node));
    cursorY += footerCard.size.height + outerPadding + 120.f;

    build.maxScrollOffset = std::max(cursorY - viewportHeight, 0.f);
    content->setSize(Size {viewportWidth, cursorY});

    viewport->appendChild(std::move(content));
    root->appendChild(std::move(viewport));
    build.root = std::move(root);
    return build;
}

class SceneGraphAnimationWindow final : public Window {
  public:
    explicit SceneGraphAnimationWindow(WindowConfig const &config)
        : Window(config) {
        SceneBuild build = buildAnimationProbeScene(Application::instance().textSystem(), m_theme);
        m_content = build.content;
        m_animatedThumbs = std::move(build.thumbs);
        m_maxScrollOffset = build.maxScrollOffset;
        m_sceneGraph.setRoot(std::move(build.root));

        updateAnimation(0.f);
        m_animationHandle = AnimationClock::instance().subscribe([this](AnimationTick const &tick) {
            updateAnimation(static_cast<float>(tick.nowSeconds));
            requestRedraw();
        });
        requestRedraw();
    }

    ~SceneGraphAnimationWindow() override {
        if (m_animationHandle.isValid()) {
            AnimationClock::instance().unsubscribe(m_animationHandle);
        }
    }

    void render(Canvas &canvas) override {
        canvas.clear(m_theme.windowBackgroundColor);
        if (!m_sceneRenderer) {
            m_sceneRenderer = std::make_unique<SceneRenderer>(canvas);
        }
        m_sceneRenderer->render(m_sceneGraph);
    }

  private:
    void updateAnimation(float timeSeconds) {
        if (m_content) {
            float const scroll = m_maxScrollOffset * oscillate(timeSeconds, 0.4f, 0.45f);
            m_content->setPosition(Point {0.f, -scroll});
        }

        for (AnimatedThumb const &thumb : m_animatedThumbs) {
            float const progress = oscillate(timeSeconds, thumb.phase, thumb.speed);
            float const x = thumb.minX + (thumb.maxX - thumb.minX) * progress;
            thumb.thumb->setPosition(Point {x, thumb.y});
        }
    }

    Theme m_theme = Theme::light();
    SceneGraph m_sceneGraph;
    GroupNode *m_content = nullptr;
    std::vector<AnimatedThumb> m_animatedThumbs;
    float m_maxScrollOffset = 0.f;
    ObserverHandle m_animationHandle;
    std::unique_ptr<SceneRenderer> m_sceneRenderer;
};

} // namespace

int main() {
    Application app;

    app.createWindow<SceneGraphAnimationWindow>({
        .size = kWindowSize,
        .title = "Scene Graph Animation",
        .resizable = false,
    });

    return app.exec();
}
