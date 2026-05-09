#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>
#include <vector>

using namespace flux;

struct OutputRoot {
  std::string title;

  auto body() const {
    return VStack{
        .spacing = 12.f,
        .alignment = Alignment::Center,
        .children = children(
            Text{
                .text = title.empty() ? std::string("Default Output") : title,
                .font = Font::largeTitle(),
                .color = Color::primary(),
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text{
                .text = title.empty() ? std::string("No named outputs reported")
                                      : std::string("WindowConfig::outputName = ") + title,
                .font = Font::body(),
                .color = Color::secondary(),
                .horizontalAlignment = HorizontalAlignment::Center,
            })
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  app.setName("Output Demo");

  std::vector<std::string> outputs = app.availableOutputs();
  if (outputs.empty()) {
    outputs.push_back({});
  }

  for (std::string const& output : outputs) {
    auto& window = app.createWindow<Window>({
        .size = {640, 420},
        .title = output.empty() ? std::string("Output Demo") : std::string("Output Demo - ") + output,
        .outputName = output,
    });
    window.setView(OutputRoot{.title = output});
  }

  return app.exec();
}
