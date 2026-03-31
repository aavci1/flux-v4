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
        FluxTheme const &t = useEnvironment<FluxTheme>();

        return VStack {
            .spacing = 16.f,
            .hAlign = HorizontalAlignment::Center,
            .children = {
                Icon {
                    .name = name,
                    .size = 32.f,
                    .weight = 600.f,
                    .color = t.colorTextSecondary
                },
                Text {
                    .text = label,
                    .font = Font {
                        .size = 16.f,
                        .weight = 400.f,
                    },
                    .color = t.colorTextSecondary,
                    .wrapping = TextWrapping::NoWrap,
                },
            },
        };
    }
};

struct IconDemoRoot {
  auto body() const {
    FluxTheme const &t = useEnvironment<FluxTheme>();

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
        .flexGrow = 1.f,
        .children =
            {
                VStack{
                    .spacing = t.space5,
                    .padding = t.space5,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{.text = "Icon demo",
                                 .font = t.typeHeading.toFont(),
                                 .color = t.colorTextPrimary,
                                 .lineHeight = t.typeHeading.lineHeight},
                            Text{
                                .text = "Material Symbols Rounded — curated "
                                        "IconName set. "
                                        "Resize the window; scroll for the "
                                        "full grid.",
                                .font = t.typeBody.toFont(),
                                .color = t.colorTextSecondary,
                                .wrapping = TextWrapping::Wrap,
                                .lineHeight = t.typeBody.lineHeight,
                            },

                            Text{.text = "All curated icons",
                                 .font = t.typeTitle.toFont(),
                                 .color = t.colorTextPrimary,
                                 .lineHeight = t.typeTitle.lineHeight},
                            Grid {
                                .columns = 8,
                                .hSpacing = 124.f,
                                .vSpacing = 124.f,
                                .hAlign = HorizontalAlignment::Center,
                                .vAlign = VerticalAlignment::Top,
                                .children = std::move(gridCells),
                            },
                        },
                },
            },
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
