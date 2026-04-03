// Typography: every Theme text style, semantic colours, wrapping, alignment, and line limits.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <string>
#include <utility>
#include <vector>

using namespace flux;

namespace {

char const* kLoremShort =
    "Resize the window to see wrapping reflow. Flux lays out UTF-8 text with the same constraints used "
    "for measurement.";

char const* kLongUnbroken =
    "Supercalifragilisticexpialidocious_pseudopseudohypoparathyroidism_abcdefghijklmnopqrstuvwxyz";

Element styleRow(Theme const& theme, char const* tokenName, TextStyle const& st, char const* sample) {
  std::vector<Element> row;
  row.push_back(Text{
                    .text = tokenName,
                    .style = theme.typeLabelSmall,
                    .color = theme.colorTextMuted,
                }
                    .size(128.f, 0.f));
  row.push_back(Text{
                    .text = sample,
                    .style = st,
                    .color = theme.colorTextPrimary,
                    .wrapping = TextWrapping::Wrap,
                }
                    .flex(1.f));
  return HStack{
      .spacing = theme.space3,
      .vAlign = VerticalAlignment::Center,
      .children = std::move(row),
  };
}

Element semanticColorsSection(Theme const& theme) {
  std::vector<Element> items;
  items.push_back(Text{
      .text = "colorTextPrimary — main ink on surfaces.",
      .style = theme.typeBody,
      .color = theme.colorTextPrimary,
      .wrapping = TextWrapping::Wrap,
  });
  items.push_back(Text{
      .text = "colorTextSecondary — de-emphasised descriptions.",
      .style = theme.typeBody,
      .color = theme.colorTextSecondary,
      .wrapping = TextWrapping::Wrap,
  });
  items.push_back(Text{
      .text = "colorTextMuted — hints and tertiary detail.",
      .style = theme.typeBody,
      .color = theme.colorTextMuted,
      .wrapping = TextWrapping::Wrap,
  });
  items.push_back(Text{
      .text = "colorAccent — links and interactive emphasis.",
      .style = theme.typeBody,
      .color = theme.colorAccent,
      .wrapping = TextWrapping::Wrap,
  });
  items.push_back(Text{
      .text = "colorDanger — destructive or error context.",
      .style = theme.typeBody,
      .color = theme.colorDanger,
      .wrapping = TextWrapping::Wrap,
  });
  return VStack{
      .spacing = theme.space2,
      .hAlign = HorizontalAlignment::Leading,
      .children = std::move(items),
  };
}

Element wrappingSection(Theme const& theme) {
  std::vector<Element> items;
  items.push_back(Text{
      .text = "Wrap (default)",
      .style = theme.typeLabel,
      .color = theme.colorTextPrimary,
  });
  items.push_back(Text{
      .text = kLoremShort,
      .style = theme.typeBody,
      .color = theme.colorTextPrimary,
      .wrapping = TextWrapping::Wrap,
  });
  items.push_back(Text{
      .text = "NoWrap — single line; may clip horizontally when space is tight.",
      .style = theme.typeLabel,
      .color = theme.colorTextPrimary,
  });
  items.push_back(Text{
      .text = kLongUnbroken,
      .style = theme.typeBodySmall,
      .color = theme.colorTextSecondary,
      .wrapping = TextWrapping::NoWrap,
  });
  items.push_back(Text{
      .text = "WrapAnywhere — breaks inside long tokens when needed.",
      .style = theme.typeLabel,
      .color = theme.colorTextPrimary,
  });
  items.push_back(Text{
      .text = kLongUnbroken,
      .style = theme.typeBodySmall,
      .color = theme.colorTextSecondary,
      .wrapping = TextWrapping::WrapAnywhere,
  });
  return VStack{
      .spacing = theme.space2,
      .hAlign = HorizontalAlignment::Leading,
      .children = std::move(items),
  };
}

Element alignBand(Theme const& theme, char const* word, HorizontalAlignment h) {
  std::vector<Element> zs;
  zs.push_back(Rectangle{
                   .fill = FillStyle::solid(theme.colorSurfaceHover),
                   .stroke = StrokeStyle::solid(theme.colorBorderSubtle, 1.f),
               }
                   .height(40.f));
  zs.push_back(Text{
      .text = word,
      .style = theme.typeBody,
      .color = theme.colorTextPrimary,
      .horizontalAlignment = h,
  }
                   .padding(8.f));
  return ZStack{
      .children = std::move(zs),
  };
}

Element alignmentSection(Theme const& theme) {
  std::vector<Element> items;
  items.push_back(alignBand(theme, "Leading", HorizontalAlignment::Leading));
  items.push_back(alignBand(theme, "Center", HorizontalAlignment::Center));
  items.push_back(alignBand(theme, "Trailing", HorizontalAlignment::Trailing));
  return VStack{
      .spacing = theme.space2,
      .children = std::move(items),
  };
}

} // namespace

struct TypographyDemoRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();

    std::vector<Element> col;
    col.push_back(Text{
        .text = "Typography",
        .style = theme.typeDisplay,
        .color = theme.colorTextPrimary,
    });
    col.push_back(Text{
        .text = "Theme text styles, semantic colours, wrapping modes, alignment, and maxLines.",
        .style = theme.typeBody,
        .color = theme.colorTextSecondary,
        .wrapping = TextWrapping::Wrap,
    });

    col.push_back(Text{
        .text = "Theme scale",
        .style = theme.typeHeading,
        .color = theme.colorTextPrimary,
    });
    col.push_back(styleRow(theme, "typeDisplay", theme.typeDisplay, "Hero / screen title"));
    col.push_back(styleRow(theme, "typeHeading", theme.typeHeading, "Major section heading"));
    col.push_back(styleRow(theme, "typeTitle", theme.typeTitle, "Card or dialog title"));
    col.push_back(styleRow(theme, "typeSubtitle", theme.typeSubtitle, "Subsection or group heading"));
    col.push_back(styleRow(theme, "typeBody", theme.typeBody, "Body paragraph — primary reading text."));
    col.push_back(styleRow(theme, "typeBodySmall", theme.typeBodySmall, "Supporting text and captions."));
    col.push_back(styleRow(theme, "typeLabel", theme.typeLabel, "Field and control labels"));
    col.push_back(styleRow(theme, "typeLabelSmall", theme.typeLabelSmall, "Compact labels and footnotes"));
    col.push_back(styleRow(theme, "typeCode", theme.typeCode, "monospace_line_height"));

    col.push_back(Text{
        .text = "Semantic colours",
        .style = theme.typeHeading,
        .color = theme.colorTextPrimary,
    });
    col.push_back(semanticColorsSection(theme));

    col.push_back(Text{
        .text = "Wrapping",
        .style = theme.typeHeading,
        .color = theme.colorTextPrimary,
    });
    col.push_back(wrappingSection(theme));

    col.push_back(Text{
        .text = "Horizontal alignment",
        .style = theme.typeHeading,
        .color = theme.colorTextPrimary,
    });
    col.push_back(alignmentSection(theme));

    col.push_back(Text{
        .text = "maxLines",
        .style = theme.typeHeading,
        .color = theme.colorTextPrimary,
    });
    col.push_back(Text{
        .text = "The paragraph below is limited to two lines (long copy is clipped for layout).",
        .style = theme.typeBodySmall,
        .color = theme.colorTextSecondary,
        .wrapping = TextWrapping::Wrap,
    });
    col.push_back(Text{
        .text = kLoremShort + std::string(" ") + kLoremShort,
        .style = theme.typeBody,
        .color = theme.colorTextPrimary,
        .wrapping = TextWrapping::Wrap,
        .maxLines = 2,
    });

    col.push_back(Spacer{}.height(theme.space4));

    Element column = VStack{
        .spacing = theme.space5,
        .hAlign = HorizontalAlignment::Leading,
        .children = std::move(col),
    }
                         .padding(theme.space5);

    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = children(std::move(column)),
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {520, 780},
      .title = "Flux — Typography",
      .resizable = true,
  });
  w.setView<TypographyDemoRoot>();
  return app.exec();
}
