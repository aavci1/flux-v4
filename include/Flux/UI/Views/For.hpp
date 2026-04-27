#pragma once

/// \file Flux/UI/Views/For.hpp
///
/// Reactive keyed list primitive for v5 build-once view trees.

#include <Flux/Reactive/Effect.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/Alignment.hpp>
#include <Flux/UI/Views/ControlFlowDetail.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

template<typename T, typename KeyFn, typename Factory>
class ForView {
public:
  using Items = std::vector<T>;
  using Key = std::decay_t<std::invoke_result_t<KeyFn&, T const&>>;
  static constexpr bool mountsWhenCollapsed = true;

  static_assert(std::equality_comparable<Key>,
                "For keys must be equality-comparable so rows can be reconciled.");

  ForView(Reactive::Signal<Items> items, KeyFn keyFn, Factory factory,
          float spacing = 0.f, Alignment alignment = Alignment::Start)
      : items_(std::move(items))
      , keyFn_(std::move(keyFn))
      , factory_(std::move(factory))
      , spacing_(spacing)
      , alignment_(alignment) {}

  Size measure(MeasureContext& ctx, LayoutConstraints const& constraints,
               LayoutHints const&, TextSystem& textSystem) const {
    ctx.advanceChildSlot();
    EnvironmentStack& environment = EnvironmentStack::current();
    std::vector<EnvironmentLayer> const environmentLayers = environment.snapshot();
    Items const items = items_.get();
    LayoutConstraints childConstraints = constraints;
    childConstraints.minWidth = 0.f;
    childConstraints.minHeight = 0.f;
    childConstraints.maxHeight = std::numeric_limits<float>::infinity();
    LayoutHints childHints{};
    childHints.vStackCrossAlign = alignment_;

    float width = 0.f;
    float height = 0.f;
    for (std::size_t index = 0; index < items.size(); ++index) {
      Reactive::Signal<std::size_t> indexSignal{index};
      auto factory = factory_;
      Element element = detail::invokeForFactory(factory, items[index], indexSignal);
      Size const measured = detail::controlMeasureElement(
          element, environment, environmentLayers, textSystem, childConstraints, childHints);
      width = std::max(width, measured.width);
      height += measured.height;
      if (index + 1 < items.size()) {
        height += spacing_;
      }
    }

    width = std::max(width, constraints.minWidth);
    height = std::max(height, constraints.minHeight);
    if (std::isfinite(constraints.maxWidth)) {
      width = std::min(width, constraints.maxWidth);
    }
    if (std::isfinite(constraints.maxHeight)) {
      height = std::min(height, constraints.maxHeight);
    }
    return Size{width, height};
  }

  std::unique_ptr<scenegraph::SceneNode> mount(MountContext& ctx) const {
    Size const frameSize{};
    auto group = std::make_unique<scenegraph::GroupNode>(
        Rect{0.f, 0.f, detail::controlFiniteOrZero(frameSize.width),
             detail::controlFiniteOrZero(frameSize.height)});
    group->setLayoutFlow(scenegraph::LayoutFlow::VerticalStack);
    group->setLayoutSpacing(spacing_);

    auto controlScope = std::make_shared<Reactive::Scope>();
    ctx.owner().onCleanup([controlScope] {
      controlScope->dispose();
    });

    auto state = std::make_shared<State>(
        items_, keyFn_, factory_, spacing_, alignment_, frameSize, ctx.environment(),
        ctx.environment().snapshot(), ctx.textSystem(), ctx.constraints(), ctx.redrawCallback());

    scenegraph::GroupNode* rawGroup = group.get();
    Reactive::withOwner(*controlScope, [state, rawGroup] {
      Reactive::Effect([state, rawGroup] {
        state->reconcile(*rawGroup);
      });
    });

    return group;
  }

private:
  struct Row {
    Key key;
    Reactive::Signal<std::size_t> index;
    std::shared_ptr<Reactive::Scope> scope;
  };

  struct State {
    Reactive::Signal<Items> items;
    KeyFn keyFn;
    Factory factory;
    float spacing = 0.f;
    Alignment alignment = Alignment::Start;
    Size frameSize{};
    EnvironmentStack& environment;
    std::vector<EnvironmentLayer> environmentLayers;
    TextSystem& textSystem;
    LayoutConstraints constraints;
    std::function<void()> requestRedraw;
    std::vector<Row> rows;

    State(Reactive::Signal<Items> itemsIn, KeyFn keyFnIn, Factory factoryIn,
          float spacingIn, Alignment alignmentIn, Size frameSizeIn,
          EnvironmentStack& environmentIn,
          std::vector<EnvironmentLayer> environmentLayersIn,
          TextSystem& textSystemIn, LayoutConstraints constraintsIn,
          std::function<void()> requestRedrawIn)
        : items(std::move(itemsIn))
        , keyFn(std::move(keyFnIn))
        , factory(std::move(factoryIn))
        , spacing(spacingIn)
        , alignment(alignmentIn)
        , frameSize(frameSizeIn)
        , environment(environmentIn)
        , environmentLayers(std::move(environmentLayersIn))
        , textSystem(textSystemIn)
        , constraints(constraintsIn)
        , requestRedraw(std::move(requestRedrawIn)) {}

    ~State() {
      disposeRows(rows);
    }

    void reconcile(scenegraph::GroupNode& group) {
      Size const oldSize = group.size();
      Items const nextItems = items.get();
      std::vector<std::unique_ptr<scenegraph::SceneNode>> oldNodes = group.releaseChildren();
      std::vector<Row> oldRows = std::move(rows);
      std::vector<bool> used(oldRows.size(), false);
      std::vector<Row> nextRows;
      std::vector<std::unique_ptr<scenegraph::SceneNode>> nextNodes;
      nextRows.reserve(nextItems.size());
      nextNodes.reserve(nextItems.size());

      for (std::size_t index = 0; index < nextItems.size(); ++index) {
        T const& item = nextItems[index];
        Key key = std::invoke(keyFn, item);
        std::optional<std::size_t> match = findUnused(oldRows, used, key);
        if (match) {
          std::size_t const oldIndex = *match;
          used[oldIndex] = true;
          Row row = std::move(oldRows[oldIndex]);
          row.index.set(index);
          nextRows.push_back(std::move(row));
          nextNodes.push_back(std::move(oldNodes[oldIndex]));
        } else {
          auto mounted = mountRow(item, index, key);
          nextRows.push_back(std::move(mounted.row));
          nextNodes.push_back(std::move(mounted.node));
        }
      }

      for (std::size_t i = 0; i < oldRows.size(); ++i) {
        if (!used[i] && oldRows[i].scope) {
          oldRows[i].scope->dispose();
        }
      }

      rows = std::move(nextRows);
      group.replaceChildren(std::move(nextNodes));
      detail::controlLayoutVertical(group, frameSize, spacing);
      detail::controlPropagateLayoutChange(group, oldSize);
      if (requestRedraw) {
        requestRedraw();
      }
    }

    static void disposeRows(std::vector<Row>& rowsToDispose) {
      for (Row& row : rowsToDispose) {
        if (row.scope) {
          row.scope->dispose();
        }
      }
      rowsToDispose.clear();
    }

    std::optional<std::size_t> findUnused(std::vector<Row> const& candidates,
                                          std::vector<bool> const& used,
                                          Key const& key) const {
      for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (!used[i] && candidates[i].key == key) {
          return i;
        }
      }
      return std::nullopt;
    }

    struct MountedRow {
      Row row;
      std::unique_ptr<scenegraph::SceneNode> node;
    };

    MountedRow mountRow(T const& item, std::size_t index, Key key) {
      return Reactive::untrack([&] {
        auto rowScope = std::make_shared<Reactive::Scope>();
        Reactive::Signal<std::size_t> indexSignal = Reactive::withOwner(*rowScope, [&] {
          return Reactive::Signal<std::size_t>(index);
        });

        auto node = Reactive::withOwner(*rowScope, [&] {
          Element element = detail::invokeForFactory(factory, item, indexSignal);
          LayoutConstraints childConstraints = constraints;
          childConstraints.minWidth = 0.f;
          childConstraints.minHeight = 0.f;
          childConstraints.maxHeight = std::numeric_limits<float>::infinity();
          LayoutHints childHints{};
          childHints.vStackCrossAlign = alignment;
          Size measured = detail::controlMeasureElement(
              element, environment, environmentLayers, textSystem, childConstraints, childHints);
          return detail::controlMountElement(
              element, *rowScope, environment, environmentLayers, textSystem,
              detail::controlFixedConstraints(measured), childHints, requestRedraw);
        });

        return MountedRow{
            .row = Row{std::move(key), std::move(indexSignal), std::move(rowScope)},
            .node = std::move(node),
        };
      });
    }
  };

  Reactive::Signal<Items> items_;
  KeyFn keyFn_;
  Factory factory_;
  float spacing_ = 0.f;
  Alignment alignment_ = Alignment::Start;
};

template<typename T, typename KeyFn, typename Factory>
ForView<T, std::decay_t<KeyFn>, std::decay_t<Factory>>
For(Reactive::Signal<std::vector<T>> items, KeyFn&& keyFn, Factory&& factory,
    float spacing = 0.f, Alignment alignment = Alignment::Start) {
  return ForView<T, std::decay_t<KeyFn>, std::decay_t<Factory>>{
      std::move(items), std::forward<KeyFn>(keyFn), std::forward<Factory>(factory),
      spacing, alignment};
}

} // namespace flux
