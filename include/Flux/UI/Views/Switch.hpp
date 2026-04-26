#pragma once

/// \file Flux/UI/Views/Switch.hpp
///
/// Reactive multi-branch primitive for v5 build-once view trees.

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

template<typename T>
struct SwitchCase {
  T value;
  std::function<Element()> factory;
};

template<typename T, typename Factory>
SwitchCase<std::decay_t<T>> Case(T&& value, Factory&& factory) {
  using Value = std::decay_t<T>;
  return SwitchCase<Value>{
      .value = std::forward<T>(value),
      .factory = [factory = std::forward<Factory>(factory)]() mutable -> Element {
        return detail::invokeElementFactory(factory);
      },
  };
}

template<typename T, typename Selector>
class SwitchView {
public:
  using Value = std::decay_t<T>;

  SwitchView(Selector selector, std::vector<SwitchCase<Value>> cases,
             std::function<Element()> defaultFactory)
      : selector_(std::move(selector))
      , cases_(std::move(cases))
      , defaultFactory_(std::move(defaultFactory)) {}

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
        selector_, cases_, defaultFactory_, frameSize, ctx.environment(),
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
    Selector selector;
    std::vector<SwitchCase<Value>> cases;
    std::function<Element()> defaultFactory;
    Size frameSize{};
    EnvironmentStack& environment;
    std::vector<EnvironmentLayer> environmentLayers;
    TextSystem& textSystem;
    LayoutConstraints constraints;
    LayoutHints hints;
    std::function<void()> requestRedraw;
    std::optional<std::size_t> activeBranch;
    std::shared_ptr<Reactive2::Scope> branchScope;

    State(Selector selectorIn, std::vector<SwitchCase<Value>> casesIn,
          std::function<Element()> defaultFactoryIn, Size frameSizeIn,
          EnvironmentStack& environmentIn,
          std::vector<EnvironmentLayer> environmentLayersIn, TextSystem& textSystemIn,
          LayoutConstraints constraintsIn, LayoutHints hintsIn,
          std::function<void()> requestRedrawIn)
        : selector(std::move(selectorIn))
        , cases(std::move(casesIn))
        , defaultFactory(std::move(defaultFactoryIn))
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
      Value const value = detail::readSelector(selector);
      std::size_t const nextBranch = branchIndex(value);
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

    std::size_t branchIndex(Value const& value) const {
      for (std::size_t i = 0; i < cases.size(); ++i) {
        if (cases[i].value == value) {
          return i;
        }
      }
      return cases.size();
    }

    void disposeBranch() {
      if (branchScope) {
        branchScope->dispose();
        branchScope.reset();
      }
      activeBranch.reset();
    }

    std::unique_ptr<scenegraph::SceneNode> mountBranch(std::size_t branch) {
      return Reactive2::untrack([&] {
        branchScope = std::make_shared<Reactive2::Scope>();
        return Reactive2::withOwner(*branchScope, [&] {
          Element element = branch < cases.size()
              ? cases[branch].factory()
              : defaultFactory();
          Size measured = detail::controlMeasureElement(
              element, environment, environmentLayers, textSystem, constraints, hints);
          return detail::controlMountElement(
              element, *branchScope, environment, environmentLayers, textSystem,
              detail::controlFixedConstraints(measured), hints, requestRedraw);
        });
      });
    }
  };

  Selector selector_;
  std::vector<SwitchCase<Value>> cases_;
  std::function<Element()> defaultFactory_;
};

template<typename Selector, typename Value = std::decay_t<std::invoke_result_t<Selector&>>>
SwitchView<Value, std::decay_t<Selector>>
Switch(Selector&& selector, std::vector<SwitchCase<Value>> cases,
       std::function<Element()> defaultFactory = [] { return Element{Spacer{}}; }) {
  return SwitchView<Value, std::decay_t<Selector>>{
      std::forward<Selector>(selector), std::move(cases), std::move(defaultFactory)};
}

} // namespace flux
