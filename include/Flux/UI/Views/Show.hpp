#pragma once

/// \file Flux/UI/Views/Show.hpp
///
/// Reactive conditional primitive for v5 build-once view trees.

#include <Flux/Reactive2/Effect.hpp>
#include <Flux/UI/Views/ControlFlowDetail.hpp>
#include <Flux/UI/Views/Spacer.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

template<typename Condition, typename ThenFactory, typename ElseFactory>
class ShowView {
public:
  ShowView(Condition condition, ThenFactory thenFactory, ElseFactory elseFactory)
      : condition_(std::move(condition))
      , thenFactory_(std::move(thenFactory))
      , elseFactory_(std::move(elseFactory)) {}

  Size measure(MeasureContext&, LayoutConstraints const& constraints,
               LayoutHints const&, TextSystem&) const {
    return detail::controlAssignedSize(constraints);
  }

  std::unique_ptr<scenegraph::SceneNode> mount(MountContext& ctx) const {
    Size const frameSize = detail::controlAssignedSize(ctx.constraints());
    auto group = std::make_unique<scenegraph::GroupNode>(
        Rect{0.f, 0.f, detail::controlFiniteOrZero(frameSize.width),
             detail::controlFiniteOrZero(frameSize.height)});

    auto controlScope = std::make_shared<Reactive2::Scope>();
    ctx.owner().onCleanup([controlScope] {
      controlScope->dispose();
    });

    auto state = std::make_shared<State>(
        condition_, thenFactory_, elseFactory_, frameSize, ctx.environment(),
        ctx.environment().snapshot(), ctx.textSystem(), ctx.constraints(), ctx.hints(),
        ctx.redrawCallback());

    scenegraph::GroupNode* rawGroup = group.get();
    Reactive2::withOwner(*controlScope, [state, rawGroup] {
      Reactive2::Effect([state, rawGroup] {
        state->reconcile(*rawGroup);
      });
    });

    return group;
  }

private:
  struct State {
    Condition condition;
    ThenFactory thenFactory;
    ElseFactory elseFactory;
    Size frameSize{};
    EnvironmentStack& environment;
    std::vector<EnvironmentLayer> environmentLayers;
    TextSystem& textSystem;
    LayoutConstraints constraints;
    LayoutHints hints;
    std::function<void()> requestRedraw;
    std::optional<bool> activeBranch;
    std::shared_ptr<Reactive2::Scope> branchScope;

    State(Condition conditionIn, ThenFactory thenFactoryIn, ElseFactory elseFactoryIn,
          Size frameSizeIn, EnvironmentStack& environmentIn,
          std::vector<EnvironmentLayer> environmentLayersIn, TextSystem& textSystemIn,
          LayoutConstraints constraintsIn, LayoutHints hintsIn,
          std::function<void()> requestRedrawIn)
        : condition(std::move(conditionIn))
        , thenFactory(std::move(thenFactoryIn))
        , elseFactory(std::move(elseFactoryIn))
        , frameSize(frameSizeIn)
        , environment(environmentIn)
        , environmentLayers(std::move(environmentLayersIn))
        , textSystem(textSystemIn)
        , constraints(constraintsIn)
        , hints(hintsIn)
        , requestRedraw(std::move(requestRedrawIn)) {}

    ~State() {
      disposeBranch();
    }

    void reconcile(scenegraph::GroupNode& group) {
      bool const nextBranch = detail::readCondition(condition);
      if (activeBranch && *activeBranch == nextBranch) {
        return;
      }

      disposeBranch();
      (void)group.releaseChildren();
      activeBranch = nextBranch;

      auto node = mountBranch(nextBranch);
      if (node) {
        std::vector<std::unique_ptr<scenegraph::SceneNode>> children;
        children.push_back(std::move(node));
        group.replaceChildren(std::move(children));
      }
      detail::controlLayoutSingle(group, frameSize);
      if (requestRedraw) {
        requestRedraw();
      }
    }

    void disposeBranch() {
      if (branchScope) {
        branchScope->dispose();
        branchScope.reset();
      }
      activeBranch.reset();
    }

    std::unique_ptr<scenegraph::SceneNode> mountBranch(bool thenBranch) {
      return Reactive2::untrack([&] {
        branchScope = std::make_shared<Reactive2::Scope>();
        return Reactive2::withOwner(*branchScope, [&] {
          Element element = thenBranch
              ? detail::invokeElementFactory(thenFactory)
              : detail::invokeElementFactory(elseFactory);
          Size measured = detail::controlMeasureElement(
              element, environment, environmentLayers, textSystem, constraints, hints);
          return detail::controlMountElement(
              element, *branchScope, environment, environmentLayers, textSystem,
              detail::controlFixedConstraints(measured), hints, requestRedraw);
        });
      });
    }
  };

  Condition condition_;
  ThenFactory thenFactory_;
  ElseFactory elseFactory_;
};

template<typename Condition, typename ThenFactory, typename ElseFactory>
ShowView<std::decay_t<Condition>, std::decay_t<ThenFactory>, std::decay_t<ElseFactory>>
Show(Condition&& condition, ThenFactory&& thenFactory, ElseFactory&& elseFactory) {
  return ShowView<std::decay_t<Condition>, std::decay_t<ThenFactory>, std::decay_t<ElseFactory>>{
      std::forward<Condition>(condition), std::forward<ThenFactory>(thenFactory),
      std::forward<ElseFactory>(elseFactory)};
}

template<typename Condition, typename ThenFactory>
auto Show(Condition&& condition, ThenFactory&& thenFactory) {
  auto empty = [] {
    return Spacer{};
  };
  return Show(std::forward<Condition>(condition), std::forward<ThenFactory>(thenFactory),
              std::move(empty));
}

} // namespace flux
