#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Theme.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string_view>

namespace {

using namespace flux;
using namespace flux::scenegraph;

constexpr std::string_view kGreeting = "Hello, World!";
constexpr auto kFrameInterval = std::chrono::nanoseconds {16'666'667};
constexpr Point kInitialVelocity {260.f, 180.f};

struct EnvironmentScope {
    explicit EnvironmentScope(EnvironmentLayer layer) {
        EnvironmentStack::current().push(std::move(layer));
    }

    ~EnvironmentScope() {
        EnvironmentStack::current().pop();
    }
};

float advanceAxis(float position, float &velocity, float limit, float deltaSeconds) {
    if (limit <= 0.f) {
        return 0.f;
    }

    position += velocity * deltaSeconds;

    while (position < 0.f || position > limit) {
        if (position < 0.f) {
            position = -position;
            velocity = std::abs(velocity);
        } else if (position > limit) {
            position = 2.f * limit - position;
            velocity = -std::abs(velocity);
        }
    }

    return std::clamp(position, 0.f, limit);
}

class SceneGraphExampleWindow final : public Window {
  public:
    explicit SceneGraphExampleWindow(WindowConfig const &config) : Window(config) {
        setEnvironmentValue<Theme>(Theme::light());

        EnvironmentScope envScope {environmentLayer()};
        buildRetainedScene(Application::instance().textSystem());

        m_subscriptionState = std::make_shared<SubscriptionState>();
        m_subscriptionState->window = this;
        m_subscriptionState->windowHandle = handle();

        subscribeToWindowEvents();

        m_animationTimerId = Application::instance().scheduleRepeatingTimer(kFrameInterval, handle());
        requestRedraw();
    }

    ~SceneGraphExampleWindow() override {
        if (Application::hasInstance() && m_animationTimerId != 0) {
            Application::instance().cancelTimer(m_animationTimerId);
        }
        if (m_subscriptionState) {
            m_subscriptionState->window = nullptr;
            m_subscriptionState.reset();
        }
    }

    void render(Canvas &canvas) override {
        EnvironmentScope envScope {environmentLayer()};
        canvas.clear(Color::windowBackground());
        SceneRenderer renderer {canvas};
        renderer.render(m_sceneGraph);
    }

  private:
    struct SubscriptionState {
        SceneGraphExampleWindow *window = nullptr;
        unsigned int windowHandle = 0;
    };

    void buildRetainedScene(TextSystem &textSystem) {
        m_viewportSize = getSize();

        auto root = std::make_unique<GroupNode>(Rect {0.f, 0.f, m_viewportSize.width, m_viewportSize.height});
        auto layout = textSystem.layout(kGreeting, Font::largeTitle(), Color::primary());

        m_textSize = layout ? layout->measuredSize : Size {};

        auto textNode = std::make_unique<TextNode>(
            Rect {0.f, 0.f, m_textSize.width, m_textSize.height},
            std::move(layout)
        );
        m_textNode = textNode.get();

        root->appendChild(std::move(textNode));
        m_sceneGraph.setRoot(std::move(root));

        setTextPosition(centeredTextPosition());
    }

    void subscribeToWindowEvents() {
        std::weak_ptr<SubscriptionState> weakState = m_subscriptionState;

        Application::instance().eventQueue().on<WindowEvent>(
            [weakState](WindowEvent const &event) {
                auto state = weakState.lock();
                if (!state || !state->window || event.handle != state->windowHandle) {
                    return;
                }
                if (event.kind == WindowEvent::Kind::Resize) {
                    state->window->handleResize(event.size);
                }
            }
        );

        Application::instance().eventQueue().on<TimerEvent>(
            [weakState](TimerEvent const &event) {
                auto state = weakState.lock();
                if (!state || !state->window || event.windowHandle != state->windowHandle) {
                    return;
                }
                if (event.timerId == state->window->m_animationTimerId) {
                    state->window->handleAnimationTick(event);
                }
            }
        );
    }

    void handleResize(Size size) {
        m_viewportSize = size;
        m_sceneGraph.root().bounds = Rect {0.f, 0.f, size.width, size.height};
        clampTextToViewport();
        m_lastTickSeconds.reset();
        requestRedraw();
    }

    void handleAnimationTick(TimerEvent const &event) {
        if (!m_textNode) {
            return;
        }

        double const nowSeconds = static_cast<double>(event.deadlineNanos) * 1e-9;
        if (!m_lastTickSeconds.has_value()) {
            m_lastTickSeconds = nowSeconds;
            requestRedraw();
            return;
        }

        float const deltaSeconds = static_cast<float>(std::clamp(nowSeconds - *m_lastTickSeconds, 0.0, 0.05));
        m_lastTickSeconds = nowSeconds;

        Point position {m_textNode->bounds.x, m_textNode->bounds.y};
        position.x = advanceAxis(position.x, m_velocity.x, maxOffsetX(), deltaSeconds);
        position.y = advanceAxis(position.y, m_velocity.y, maxOffsetY(), deltaSeconds);

        setTextPosition(position);
        requestRedraw();
    }

    Point centeredTextPosition() const {
        return {
            std::max((m_viewportSize.width - m_textSize.width) * 0.5f, 0.f),
            std::max((m_viewportSize.height - m_textSize.height) * 0.5f, 0.f),
        };
    }

    float maxOffsetX() const {
        return std::max(m_viewportSize.width - m_textSize.width, 0.f);
    }

    float maxOffsetY() const {
        return std::max(m_viewportSize.height - m_textSize.height, 0.f);
    }

    void clampTextToViewport() {
        if (!m_textNode) {
            return;
        }

        Point position {
            std::clamp(m_textNode->bounds.x, 0.f, maxOffsetX()),
            std::clamp(m_textNode->bounds.y, 0.f, maxOffsetY()),
        };
        setTextPosition(position);

        if (position.x <= 0.f) {
            m_velocity.x = std::abs(m_velocity.x);
        } else if (position.x >= maxOffsetX()) {
            m_velocity.x = -std::abs(m_velocity.x);
        }

        if (position.y <= 0.f) {
            m_velocity.y = std::abs(m_velocity.y);
        } else if (position.y >= maxOffsetY()) {
            m_velocity.y = -std::abs(m_velocity.y);
        }
    }

    void setTextPosition(Point position) {
        if (!m_textNode) {
            return;
        }
        m_textNode->bounds = Rect {position.x, position.y, m_textSize.width, m_textSize.height};
    }

    SceneGraph m_sceneGraph;
    TextNode *m_textNode = nullptr;
    Size m_viewportSize {};
    Size m_textSize {};
    Point m_velocity = kInitialVelocity;
    std::optional<double> m_lastTickSeconds;
    std::shared_ptr<SubscriptionState> m_subscriptionState;
    std::uint64_t m_animationTimerId = 0;
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
