#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "ChatsView.hpp"
#include "Divider.hpp"
#include "ModelsView.hpp"
#include "SettingsView.hpp"
#include "Sidebar.hpp"

using namespace flux;
using namespace lambda;

struct LambdaStudio : ViewModifiers<LambdaStudio> {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto selectedModule = useState<std::string>("Chats");

        Element currentView = ChatsView {}.flex(1.f, 1.f);
        if (*selectedModule == "Models") {
            currentView = ModelsView {}.flex(1.f, 1.f);
        } else if (*selectedModule == "Settings") {
            currentView = SettingsView {}.flex(1.f, 1.f);
        }

        return HStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                Sidebar {
                    .modules = {
                        {IconName::ChatBubble, "Chats"},
                        {IconName::ModelTraining, "Models"},
                        {IconName::Settings, "Settings"}
                    },
                    .selectedTitle = *selectedModule,
                    .onSelect = [selectedModule](std::string title) {
                        selectedModule = std::move(title);
                    },
                }
                    .flex(0.f, 0.f),
                Divider {.orientation = Divider::Orientation::Vertical}, std::move(currentView)
            ),
        };
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto &w = app.createWindow<Window>({
        .size = {1100, 720},
        .title = "Lambda Studio",
        .resizable = true,
    });

    w.setView<LambdaStudio>();

    return app.exec();
}
