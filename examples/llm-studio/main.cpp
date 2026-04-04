#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "ChatArea.hpp"
#include "Divider.hpp"
#include "Menu.hpp"
#include "MenuPanel.hpp"
#include "Message.hpp"
#include "MessageBox.hpp"
#include "OllamaClient.hpp"
#include "PropertiesPanel.hpp"

using namespace flux;

struct AppRoot : ViewModifiers<AppRoot> {
    auto body() const {
        return HStack {
            .spacing = 8.f,
            .alignment = Alignment::Stretch,
            .children = children(
                MenuPanel{},
                ChatArea{}.flex(1.f, 1.f, 400.f),
                PropertiesPanel{}
            ),
        }.padding(16.f, 0.f, 16.f, 0.f);
    }
};

int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    Application app(argc, argv);

    auto& w = app.createWindow<Window>({
        .size = {1280, 800},
        .title = "LLM Studio",
    });

    w.setView(AppRoot {});

    int const code = app.exec();
    curl_global_cleanup();
    return code;
}
