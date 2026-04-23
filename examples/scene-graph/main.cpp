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
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Theme.hpp>

#include <memory>
#include <string_view>

namespace {

using namespace flux;
using namespace flux::scenegraph;

struct EnvironmentScope {
    explicit EnvironmentScope(EnvironmentLayer layer) {
        EnvironmentStack::current().push(std::move(layer));
    }
    ~EnvironmentScope() {
        EnvironmentStack::current().pop();
    }
};

class SceneGraphExampleWindow final : public Window {
  public:
    explicit SceneGraphExampleWindow(WindowConfig const &config) : Window(config) {
        setEnvironmentValue<Theme>(Theme::light());
        EnvironmentScope envScope {environmentLayer()};
        this->m_sceneGraph = this->buildSceneGraph(this->getSize(), Application::instance().textSystem());
        requestRedraw();
    }

    SceneGraph buildSceneGraph(Size size, TextSystem &textSystem) {
        auto const theme = environmentValue<Theme>();

        auto root = std::make_unique<GroupNode>(Rect {0.f, 0.f, size.width, size.height});

        auto textBounds = Rect {
            theme->space5, 
            theme->space5, 
            size.width - theme->space5 * 2.f, 
            size.height - theme->space5 * 2.f
        };

        auto textOptions = TextLayoutOptions { 
            .horizontalAlignment = HorizontalAlignment::Center, 
            .verticalAlignment = VerticalAlignment::Center 
        };

        auto layout = textSystem.layout("Hello, World!", Font::largeTitle(), Color::primary(), textBounds, textOptions);

        root->appendChild(std::make_unique<TextNode>(
            textBounds, 
            layout
        ));

        return SceneGraph {std::move(root)};
    }

    void render(Canvas &canvas) override {
        canvas.clear(Color::windowBackground());
        SceneRenderer renderer {canvas};
        renderer.render(this->m_sceneGraph);
    }

private:
    SceneGraph m_sceneGraph;
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
