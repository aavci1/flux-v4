#include "EditorDocument.hpp"

#include <Lambda.hpp>
#include <Lambda/UI/Shortcut.hpp>
#include <Lambda/UI/Theme.hpp>
#include <Lambda/UI/UI.hpp>
#include <Lambda/UI/Views/Views.hpp>

#include <string>
#include <utility>

using namespace lambda;
using namespace lambda_editor;

namespace {

struct LambdaEditor {
  EditorDocument initialDocument = EditorDocument::untitled();
  std::string initialPathText;
  std::string initialStatus;

  Element body() const {
    auto theme = useEnvironment<ThemeKey>();
    auto document = useState(initialDocument);
    auto path = useState(initialPathText.empty() ? initialDocument.pathText() : initialPathText);
    auto text = useState(initialDocument.text());
    auto status = useState(initialStatus.empty() ? std::string{"Ready"} : initialStatus);

    auto openFile = [document, path, text, status] {
      EditorDocumentResult result = openDocument(path.peek());
      status.set(result.status);
      if (result.ok) {
        document.set(result.document);
        path.set(result.document.pathText());
        text.set(result.document.text());
      }
    };
    auto saveFile = [document, path, text, status] {
      EditorDocument current = document.peek();
      current.setText(text.peek());
      EditorDocumentResult result =
          current.hasPath() && path.peek() == current.pathText()
              ? saveDocument(current)
              : saveDocumentAs(current, path.peek());
      status.set(result.status);
      if (result.ok) {
        document.set(result.document);
        path.set(result.document.pathText());
        text.set(result.document.text());
      }
    };
    auto newFile = [document, path, text, status] {
      document.set(EditorDocument::untitled());
      path.set("");
      text.set("");
      status.set("New document");
    };

    TextInput::Style pathStyle;
    pathStyle.font = Font{.size = 13.f, .weight = 450.f};
    pathStyle.height = 34.f;

    TextInput::Style editorStyle = TextInput::Style::plain();
    editorStyle.font = Font{.family = "monospace", .size = 14.f};
    editorStyle.textColor = Color::primary();
    editorStyle.placeholderColor = Color::secondary();
    editorStyle.paddingH = 12.f;
    editorStyle.paddingV = 12.f;
    editorStyle.lineHeight = 20.f;

    Button::Style buttonStyle;
    buttonStyle.font = Font{.size = 12.f, .weight = 650.f};
    buttonStyle.paddingH = 12.f;
    buttonStyle.paddingV = 7.f;
    buttonStyle.cornerRadius = 7.f;

    return VStack{
               .spacing = 0.f,
               .alignment = Alignment::Stretch,
               .children = children(
                   HStack{
                       .spacing = theme().space3,
                       .alignment = Alignment::Center,
                       .children = children(
                           Text{
                               .text = [document] {
                                 EditorDocument const& current = document();
                                 return current.displayName() + (current.isDirty() ? " *" : "");
                               },
                               .font = Font{.size = 15.f, .weight = 650.f},
                               .color = Color::primary(),
                               .verticalAlignment = VerticalAlignment::Center,
                           }.width(180.f),
                           TextInput{
                               .value = path,
                               .placeholder = "/path/to/file.txt",
                               .style = pathStyle,
                               .onSubmit = [openFile](std::string const&) { openFile(); },
                           }.flex(1.f, 1.f, 0.f),
                           Button{.label = "New",
                                  .variant = ButtonVariant::Secondary,
                                  .style = buttonStyle,
                                  .onTap = newFile},
                           Button{.label = "Open",
                                  .variant = ButtonVariant::Secondary,
                                  .style = buttonStyle,
                                  .onTap = openFile},
                           Button{.label = "Save",
                                  .variant = ButtonVariant::Primary,
                                  .style = buttonStyle,
                                  .onTap = saveFile})}
                       .padding(theme().space3)
                       .fill(FillStyle::solid(Color::windowBackground()))
                       .stroke(StrokeStyle::solid(Color::separator(), 1.f)),
                   TextInput{
                       .value = text,
                       .placeholder = "Start typing...",
                       .style = editorStyle,
                       .multiline = true,
                       .multilineHeight = {.fixed = 0.f, .minIntrinsic = 560.f},
                       .onChange = [document](std::string const& value) {
                         EditorDocument current = document.peek();
                         current.setText(value);
                         document.set(std::move(current));
                       },
                   }.flex(1.f, 1.f, 0.f)
                       .fill(FillStyle::solid(Color::controlBackground())),
                   HStack{
                       .spacing = theme().space2,
                       .alignment = Alignment::Center,
                       .children = children(
                           Text{
                               .text = status,
                               .font = Font{.size = 12.f, .weight = 450.f},
                               .color = Color::secondary(),
                               .verticalAlignment = VerticalAlignment::Center,
                           }.flex(1.f, 1.f, 0.f),
                           Text{
                               .text = [text] { return std::to_string(text().size()) + " chars"; },
                               .font = Font{.size = 12.f, .weight = 450.f},
                               .color = Color::secondary(),
                               .verticalAlignment = VerticalAlignment::Center,
                           })}
                       .padding(8.f, theme().space3, 8.f, theme().space3)
                       .fill(FillStyle::solid(Color::windowBackground()))
                       .stroke(StrokeStyle::solid(Color::separator(), 1.f)))};
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  std::string initialPath = argc > 1 ? std::string(argv[1]) : std::string{};
  EditorDocumentResult initial =
      initialPath.empty() ? EditorDocumentResult{} : openDocument(initialPath);

  auto& window = app.createWindow<Window>({
      .size = {920.f, 720.f},
      .title = "Lambda Editor",
      .resizable = true,
  });
  window.registerAction("edit.copy", {.label = "Copy", .shortcut = shortcuts::Copy});
  window.registerAction("edit.cut", {.label = "Cut", .shortcut = shortcuts::Cut});
  window.registerAction("edit.paste", {.label = "Paste", .shortcut = shortcuts::Paste});
  window.registerAction("edit.selectAll", {.label = "Select All", .shortcut = shortcuts::SelectAll});
  window.registerAction("app.quit", {.label = "Quit", .shortcut = shortcuts::Quit, .isEnabled = [] { return true; }});
  window.setView<LambdaEditor>({
      .initialDocument = initial.ok ? std::move(initial.document) : EditorDocument::untitled(),
      .initialPathText = initial.ok ? std::string{} : std::move(initialPath),
      .initialStatus = initial.status,
  });
  return app.exec();
}
