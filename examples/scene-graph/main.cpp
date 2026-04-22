#include <Flux/Core/Application.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>

#include <memory>
#include <string_view>

namespace {

using namespace flux;
using namespace flux::scenegraph;

Font makeFont(float size, float weight) {
    Font font;
    font.size = size;
    font.weight = weight;
    return font;
}

ShadowStyle makeShadow(float radius, Point offset, Color color) {
    ShadowStyle shadow;
    shadow.radius = radius;
    shadow.offset = offset;
    shadow.color = color;
    return shadow;
}

std::shared_ptr<TextLayout const> layoutText(TextSystem &textSystem, std::string_view text, Font const &font, Color color, Rect box, TextLayoutOptions options = {}) {
    return textSystem.layout(text, font, color, box, options);
}

void addChip(GroupNode &parent, TextSystem &textSystem, Rect bounds, std::string_view label, Color fill, Color textColor) {
    auto chip = std::make_unique<GroupNode>(bounds);
    chip->appendChild(std::make_unique<RectNode>(
        Rect {0.f, 0.f, bounds.width, bounds.height}, FillStyle::solid(fill), StrokeStyle::none(), CornerRadius {18.f}
    ));

    chip->appendChild(std::make_unique<TextNode>(
        Rect {14.f, 9.f, bounds.width - 28.f, bounds.height - 18.f},
        layoutText(textSystem, label, makeFont(14.f, 600.f), textColor, Rect {0.f, 0.f, bounds.width - 28.f, bounds.height - 18.f}, TextLayoutOptions {.wrapping = TextWrapping::NoWrap})
    ));

    parent.appendChild(std::move(chip));
}

SceneGraph buildSceneGraph(Size windowSize, TextSystem &textSystem) {
    auto root = std::make_unique<GroupNode>(Rect {0.f, 0.f, windowSize.width, windowSize.height});
    root->appendChild(std::make_unique<RectNode>(
        Rect {0.f, 0.f, windowSize.width, windowSize.height}, FillStyle::solid(Color::hex(0xF4EFE8))
    ));

    float const cardWidth = windowSize.width - 112.f;
    float const cardHeight = windowSize.height - 120.f;
    auto &card = root->emplaceChild<GroupNode>(Rect {56.f, 60.f, cardWidth, cardHeight});
    card.appendChild(std::make_unique<RectNode>(
        Rect {0.f, 0.f, cardWidth, cardHeight}, FillStyle::solid(Color::hex(0xFFFBF5)),
        StrokeStyle::solid(Color::hex(0xD8CDBE), 1.f), CornerRadius {30.f},
        makeShadow(24.f, Point {0.f, 14.f}, Color {0.f, 0.f, 0.f, 0.14f})
    ));
    card.appendChild(std::make_unique<RectNode>(
        Rect {0.f, 0.f, 10.f, cardHeight}, FillStyle::solid(Color::hex(0xD77A61)), StrokeStyle::none(),
        CornerRadius {30.f, 0.f, 0.f, 30.f}
    ));

    card.appendChild(std::make_unique<TextNode>(
        Rect {32.f, 30.f, cardWidth - 64.f, 42.f},
        layoutText(textSystem, "Pure SceneGraph", makeFont(34.f, 700.f), Color::hex(0x201A17),
                   Rect {0.f, 0.f, cardWidth - 64.f, 42.f}, TextLayoutOptions {.wrapping = TextWrapping::NoWrap})
    ));

    TextLayoutOptions bodyOptions;
    bodyOptions.wrapping = TextWrapping::Wrap;
    card.appendChild(std::make_unique<TextNode>(
        Rect {32.f, 86.f, cardWidth - 64.f, 72.f},
        layoutText(textSystem,
                   "Nodes own their render data and their bounds. SceneRenderer just walks the tree, "
                   "translates into parent space, and asks each node to draw itself.",
                   makeFont(18.f, 420.f), Color::hex(0x5A514B), Rect {0.f, 0.f, cardWidth - 64.f, 72.f}, bodyOptions)
    ));

    float const dividerWidth = cardWidth - 64.f;
    card.appendChild(std::make_unique<LineNode>(
        Rect {32.f, 180.f, dividerWidth, 1.f}, Point {0.f, 0.5f}, Point {dividerWidth, 0.5f},
        StrokeStyle::solid(Color::hex(0xDCCFC2), 1.f)
    ));

    auto &detailPanel = card.emplaceChild<GroupNode>(Rect {32.f, 208.f, cardWidth - 64.f, 112.f});
    detailPanel.appendChild(std::make_unique<RectNode>(
        Rect {0.f, 0.f, detailPanel.bounds.width, detailPanel.bounds.height}, FillStyle::solid(Color::hex(0xF2E8DA)),
        StrokeStyle::none(), CornerRadius {22.f}
    ));
    detailPanel.appendChild(std::make_unique<RectNode>(
        Rect {18.f, 18.f, 74.f, 74.f}, FillStyle::solid(Color::hex(0x1E6F5C)), StrokeStyle::none(), CornerRadius {20.f}
    ));
    detailPanel.appendChild(std::make_unique<TextNode>(
        Rect {116.f, 22.f, detailPanel.bounds.width - 136.f, 28.f},
        layoutText(textSystem, "Traversal is recursive and local.", makeFont(20.f, 640.f), Color::hex(0x1F1B16),
                   Rect {0.f, 0.f, detailPanel.bounds.width - 136.f, 28.f},
                   TextLayoutOptions {.wrapping = TextWrapping::NoWrap})
    ));
    detailPanel.appendChild(std::make_unique<TextNode>(
        Rect {116.f, 56.f, detailPanel.bounds.width - 136.f, 34.f},
        layoutText(textSystem, "The example window bypasses the retained UI pipeline entirely and renders the graph directly.",
                   makeFont(15.f, 420.f), Color::hex(0x5F554E), Rect {0.f, 0.f, detailPanel.bounds.width - 136.f, 34.f},
                   bodyOptions)
    ));

    auto &chips = card.emplaceChild<GroupNode>(Rect {32.f, cardHeight - 60.f, 320.f, 36.f});
    addChip(chips, textSystem, Rect {0.f, 0.f, 126.f, 36.f}, "Rect Node", Color::hex(0xE6D5C3), Color::hex(0x4A403A));
    addChip(chips, textSystem, Rect {138.f, 0.f, 126.f, 36.f}, "Text Node", Color::hex(0xD5E7DE), Color::hex(0x264B3F));

    return SceneGraph {std::move(root)};
}

class SceneGraphExampleWindow final : public Window {
  public:
    explicit SceneGraphExampleWindow(WindowConfig const &config) : Window(config), scene_(buildSceneGraph(config.size, Application::instance().textSystem())) {
        requestRedraw();
    }

    void render(Canvas &canvas) override {
        canvas.clear(Color::hex(0xF4EFE8));
        SceneRenderer renderer {canvas};
        renderer.render(scene_);
    }

  private:
    SceneGraph scene_;
};

} // namespace

int main() {
    Application app;

    app.createWindow<SceneGraphExampleWindow>({
        .size = {840.f, 560.f},
        .title = "Scene Graph",
    });

    return app.exec();
}
