// Showcases Flux Icon (Material Symbols Rounded): grid of all curated icons,
// size scale, semantic colours.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <vector>

using namespace flux;

namespace {

struct NamedIcon {
  IconName name;
  std::string label;
};

// Same order as IconName.hpp (grouped for the demo UI).
constexpr NamedIcon kIcons[] = {
    {IconName::ArrowBack, "ArrowBack"},
    {IconName::ArrowForward, "ArrowForward"},
    {IconName::ChevronLeft, "ChevronLeft"},
    {IconName::ChevronRight, "ChevronRight"},
    {IconName::ExpandMore, "ExpandMore"},
    {IconName::ExpandLess, "ExpandLess"},
    {IconName::Menu, "Menu"},
    {IconName::MoreHoriz, "MoreHoriz"},
    {IconName::MoreVert, "MoreVert"},

    {IconName::Add, "Add"},
    {IconName::Check, "Check"},
    {IconName::Close, "Close"},
    {IconName::ContentCopy, "ContentCopy"},
    {IconName::ContentCut, "ContentCut"},
    {IconName::ContentPaste, "ContentPaste"},
    {IconName::Delete, "Delete"},
    {IconName::Edit, "Edit"},
    {IconName::Redo, "Redo"},
    {IconName::Save, "Save"},
    {IconName::Search, "Search"},
    {IconName::Undo, "Undo"},

    {IconName::CheckCircle, "CheckCircle"},
    {IconName::Error, "Error"},
    {IconName::Info, "Info"},
    {IconName::Warning, "Warning"},

    {IconName::Description, "Description"},
    {IconName::Folder, "Folder"},
    {IconName::FolderOpen, "FolderOpen"},
    {IconName::Home, "Home"},
    {IconName::Settings, "Settings"},

    {IconName::FormatBold, "FormatBold"},
    {IconName::FormatItalic, "FormatItalic"},
    {IconName::FormatUnderlined, "FormatUnderlined"},
    {IconName::FormatListBulleted, "FormatListBulleted"},
    {IconName::FormatListNumbered, "FormatListNumbered"},

    {IconName::DarkMode, "DarkMode"},
    {IconName::LightMode, "LightMode"},
    {IconName::Visibility, "Visibility"},
    {IconName::VisibilityOff, "VisibilityOff"},
};

struct IconCell {
    IconName name;
    std::string label;

    auto body() const {
        Theme const &t = useEnvironment<Theme>();

        return VStack {
            .spacing = 16.f,
            .alignment = Alignment::Center,
            .children = children(
                Icon {
                    .name = name,
                    .size = 32.f,
                    .weight = 600.f,
                    .color = t.colorTextSecondary
                },
                Text {
                    .text = label,
                    .font = t.fontBody,
                    .color = t.colorTextSecondary,
                    .wrapping = TextWrapping::NoWrap,
                }
            ),
        };
    }
};

struct IconDemoRoot {
  auto body() const {
    Theme const &t = useEnvironment<Theme>();

    std::vector<Element> gridCells;
    gridCells.reserve(sizeof(kIcons) / sizeof(kIcons[0]));
    for (NamedIcon const &item : kIcons) {
      gridCells.push_back(
        IconCell {
            .name = item.name,
            .label = item.label,
        }
      );
    }

    return ScrollView {
        .axis = ScrollAxis::Vertical,
        .children = children(
                VStack{
                    .spacing = t.space5,
                    .alignment = Alignment::Start,
                    .children = children(
                            Text{.text = "Icon demo",
                                 .font = t.fontDisplay,
                                 .color = t.colorTextPrimary},
                            Text{
                                .text = "Material Symbols Rounded — curated "
                                        "IconName set. "
                                        "Resize the window; scroll for the "
                                        "full grid.",
                                .font = t.fontBody,
                                .color = t.colorTextSecondary,
                                .wrapping = TextWrapping::Wrap,
                            },

                            Text{.text = "All curated icons",
                                 .font = t.fontHeading,
                                 .color = t.colorTextPrimary},
                            Grid {
                                .columns = 8,
                                .horizontalSpacing = 124.f,
                                .verticalSpacing = 124.f,
                                .horizontalAlignment = Alignment::Center,
                                .verticalAlignment = Alignment::Start,
                                .children = std::move(gridCells),
                            }
                        ),
                }.padding(t.space5)
            ),
    };
  }
};

} // namespace

int main(int argc, char *argv[]) {
  Application app(argc, argv);
  auto &w = app.createWindow<Window>({
      .size = {880, 720},
      .title = "Flux — Icon demo",
      .resizable = true,
  });
  w.setView<IconDemoRoot>();
  return app.exec();
}
