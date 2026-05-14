#include <Flux.hpp>
#include <Flux/UI/WindowUI.hpp>

#include "RootView.hpp"
#include "Theme.hpp"

int main(int argc, char* argv[]) {
    Application app(argc, argv);
    auto& window = app.createWindow<Window>({
        .size = {car::kWindowWidth, car::kWindowHeight},
        .title = "Car Multimedia",
        .resizable = true,
    });
    window.setTheme(car::makeCarTheme());
    window.setView<car::RootView>();
    return app.exec();
}
