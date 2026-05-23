#pragma once

#include "FilesFlowGridLayout.hpp"
#include "FilesGlyphs.hpp"
#include "FilesStore.hpp"
#include "FilesTheme.hpp"

#include <Flux/Reactive/Effect.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>
#include <Flux/UI/Views/For.hpp>
#include <Flux/UI/Views/HStack.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace lambda_files {

/// Folder grid: `For` of rows (`HStack` of tiles) in a vertical stack. Scroll height comes from
/// `FilesFlowGridLayout` so it stays aligned with the entry count (not overlay extents).
struct FilesFlowGrid : flux::ViewModifiers<FilesFlowGrid> {
  flux::Reactive::Signal<std::vector<FileEntry>> entries;
  flux::Reactive::Signal<std::string> listingKey;
  flux::Reactive::Signal<std::string> selectedPath;
  std::function<void(FileEntry const&)> activateEntry;

  float cellWidth = FilesTheme::kGridMinCell;
  float cellHeight = FilesTheme::kGridTileH;
  float horizontalSpacing = FilesTheme::kGridGapH;
  float verticalSpacing = FilesTheme::kGridGapV;

  FilesFlowGridLayout layoutMetrics() const {
    return FilesFlowGridLayout{
        .cellWidth = cellWidth,
        .cellHeight = cellHeight,
        .horizontalSpacing = horizontalSpacing,
        .verticalSpacing = verticalSpacing,
    };
  }

  flux::Size measure(flux::MeasureContext& ctx, flux::LayoutConstraints const& constraints,
                     flux::LayoutHints const&, flux::TextSystem&) const {
    (void)ctx;
    FilesFlowGridLayout const metrics = layoutMetrics();
    std::size_t const count = entries.peek().size();
    float const width = resolvedLayoutWidth(constraints);
    return metrics.contentSizeFor(width, count);
  }

  std::unique_ptr<flux::scenegraph::SceneNode> mount(flux::MountContext& ctx) const;

private:
  struct RowDescriptor {
    std::size_t rowIndex = 0;
    int columns = 0;
    std::string key;
    std::vector<FileEntry> entries;

    bool operator==(RowDescriptor const& other) const = default;
  };

  static float resolvedLayoutWidth(flux::LayoutConstraints const& constraints) {
    if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
      return constraints.maxWidth;
    }
    if (std::isfinite(constraints.minWidth) && constraints.minWidth > 0.f) {
      return constraints.minWidth;
    }
    return 0.f;
  }

  struct LayoutState {
    float layoutWidth = 0.f;
    FilesFlowGridLayout metrics;

    int columns() const { return metrics.columnCountForWidth(layoutWidth); }
  };

  flux::Element gridContentElement(flux::Reactive::Signal<std::vector<RowDescriptor>> const& rows) const;
};

inline flux::Element FilesFlowGrid::gridContentElement(
    flux::Reactive::Signal<std::vector<RowDescriptor>> const& rows) const {
  flux::Reactive::Signal<std::string> const selectedPathSignal = selectedPath;
  auto const activate = activateEntry;
  float const tileW = cellWidth;
  float const tileH = cellHeight;
  float const gapH = horizontalSpacing;

  return flux::Element{flux::For(
      rows,
      [](RowDescriptor const& row) {
        return row.key;
      },
      [selectedPathSignal, activate, tileW, tileH, gapH](
          RowDescriptor const& row, flux::Signal<std::size_t> const&) {
        int const colCount = std::max(1, row.columns);
        std::vector<flux::Element> cells;
        cells.reserve(static_cast<std::size_t>(colCount));
        for (FileEntry const& entry : row.entries) {
          flux::Reactive::Bindable<bool> selected{
              [selectedPathSignal, entry] {
                return selectedPathSignal() == entry.path.string();
              }};
          cells.push_back(flux::Element{FileItemTile{
                                            .entry = entry,
                                            .selected = selected,
                                            .onActivate = [activate, entry] { activate(entry); },
                                        }}
                              .size(tileW, tileH)
                              .clipContent(true));
        }
        return flux::HStack{
            .spacing = gapH,
            .alignment = flux::Alignment::Start,
            .children = std::move(cells),
        };
      },
      verticalSpacing,
      flux::Alignment::Start,
      flux::ForLayout::VerticalStack)};
}

inline std::unique_ptr<flux::scenegraph::SceneNode> FilesFlowGrid::mount(flux::MountContext& ctx) const {
  auto layout = std::make_shared<LayoutState>();
  layout->metrics = layoutMetrics();
  layout->layoutWidth = resolvedLayoutWidth(ctx.constraints());

  auto scope = std::make_shared<flux::Reactive::Scope>();
  flux::Reactive::Signal<std::vector<RowDescriptor>> rows =
      flux::Reactive::Signal<std::vector<RowDescriptor>>{};
  flux::Reactive::Signal<std::vector<FileEntry>> entriesSignal = entries;
  flux::Reactive::Signal<std::string> listingKeySignal = listingKey;

  auto makeRows = [layout, entriesSignal, listingKeySignal]() {
    std::vector<FileEntry> const& currentEntries = entriesSignal.peek();
    std::string const& currentListingKey = listingKeySignal.peek();
    int const columns = layout->columns();
    std::vector<RowDescriptor> nextRows;
    int const rowCount = layout->metrics.rowCountForEntries(currentEntries.size(), columns);
    nextRows.reserve(static_cast<std::size_t>(rowCount));
    for (int row = 0; row < rowCount; ++row) {
      std::size_t const baseIndex = static_cast<std::size_t>(row) * static_cast<std::size_t>(columns);
      std::size_t const endIndex =
          std::min(baseIndex + static_cast<std::size_t>(columns), currentEntries.size());
      std::vector<FileEntry> rowEntries;
      rowEntries.reserve(endIndex - baseIndex);
      std::string rowKey = currentListingKey + ":" + std::to_string(columns) + ":" + std::to_string(row);
      for (std::size_t index = baseIndex; index < endIndex; ++index) {
        FileEntry const& entry = currentEntries[index];
        rowKey += "\x1f";
        rowKey += entry.path.string();
        rowKey += entry.isDirectory ? ":d:" : ":f:";
        rowKey += std::to_string(static_cast<int>(entry.visualKind));
        rowKey += ":";
        rowKey += std::to_string(entry.size);
        rowEntries.push_back(entry);
      }
      nextRows.push_back(RowDescriptor{
          .rowIndex = static_cast<std::size_t>(row),
          .columns = columns,
          .key = std::move(rowKey),
          .entries = std::move(rowEntries),
      });
    }
    return nextRows;
  };

  auto syncRows = [rows, makeRows]() {
    rows.set(makeRows());
  };
  syncRows();

  flux::Element const content = gridContentElement(rows);

  std::unique_ptr<flux::scenegraph::SceneNode> contentNode = flux::Reactive::withOwner(*scope, [&] {
    flux::LayoutConstraints childConstraints = ctx.constraints();
    childConstraints.maxWidth = layout->layoutWidth;
    childConstraints.minHeight = 0.f;
    childConstraints.maxHeight = std::numeric_limits<float>::infinity();
    flux::MountContext childCtx = ctx.childWithOwnScope(childConstraints, ctx.hints());
    return content.mount(childCtx);
  });

  flux::Size const initialSize =
      layout->metrics.contentSizeFor(layout->layoutWidth, entries.peek().size());
  auto group =
      std::make_unique<flux::scenegraph::SceneNode>(flux::Rect{0.f, 0.f, initialSize.width, initialSize.height});
  flux::scenegraph::SceneNode* rawContent = contentNode.get();
  group->appendChild(std::move(contentNode));

  ctx.owner().onCleanup([scope] { scope->dispose(); });

  flux::scenegraph::SceneNode* rawGroup = group.get();
  rawGroup->setLayoutConstraints(ctx.constraints());
  rawGroup->setRelayout([rawGroup, rawContent, layout, entriesSignal, syncRows](
                            flux::LayoutConstraints const& constraints) {
    rawGroup->setLayoutConstraints(constraints);
    float const nextLayoutWidth = resolvedLayoutWidth(constraints);
    if (nextLayoutWidth > 0.f || layout->layoutWidth <= 0.f) {
      layout->layoutWidth = nextLayoutWidth;
    }
    syncRows();

    flux::LayoutConstraints childConstraints = constraints;
    childConstraints.maxWidth = layout->layoutWidth;
    childConstraints.minHeight = 0.f;
    childConstraints.maxHeight = std::numeric_limits<float>::infinity();
    if (rawContent) {
      (void)rawContent->relayout(childConstraints);
    }

    flux::Size const contentSize =
        layout->metrics.contentSizeFor(layout->layoutWidth, entriesSignal.peek().size());
    rawGroup->setSize(contentSize);
  });

  flux::Reactive::withOwner(*scope, [rawGroup, entriesSignal, listingKeySignal, syncRows] {
    flux::Reactive::Effect([rawGroup, entriesSignal, listingKeySignal, syncRows] {
      (void)entriesSignal();
      (void)listingKeySignal();
      syncRows();
      if (rawGroup) {
        (void)rawGroup->relayoutStoredConstraints();
      }
    });
  });

  return group;
}

} // namespace lambda_files
