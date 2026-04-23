#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/UI/Theme.hpp>

#include "../scene-graph/ToggleDemoScene.hpp"

#include <memory>

namespace {

using namespace flux;
using namespace flux::examples::scenegraphdemo;
using namespace flux::scenegraph;

class SceneGraphRefreshWindow final : public Window {
  public:
    explicit SceneGraphRefreshWindow(WindowConfig const &config)
        : Window(config) {
        m_sceneGraph.setRoot(buildToggleDemoScene(Application::instance().textSystem(), m_theme));
        m_animationHandle = AnimationClock::instance().subscribe([this](AnimationTick const &) {
            requestRedraw();
        });
        requestRedraw();
    }

    ~SceneGraphRefreshWindow() override {
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
    Theme m_theme = Theme::light();
    SceneGraph m_sceneGraph;
    ObserverHandle m_animationHandle;
    std::unique_ptr<SceneRenderer> m_sceneRenderer;
};

} // namespace

int main() {
    Application app;

    app.createWindow<SceneGraphRefreshWindow>({
        .size = kWindowSize,
        .title = "Scene Graph Refresh",
        .resizable = false,
    });

    return app.exec();
}
