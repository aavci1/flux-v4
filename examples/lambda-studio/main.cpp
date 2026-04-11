#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>

using namespace flux;

struct LambdaStudio {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        return Text {
            .text = "Lambda Studio",
            .font = theme.fontDisplay,
            .color = theme.colorTextPrimary,
            .horizontalAlignment = HorizontalAlignment::Center,
            .verticalAlignment = VerticalAlignment::Center,
        };
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto &w = app.createWindow<Window>({
        .size = {320, 320},
        .title = "Lambda Studio",
    });

    w.setView<LambdaStudio>();

    return app.exec();
}
