#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

#include "LambdaStudioInterfaces.hpp"

namespace lambda {

struct LambdaStudioRuntime {
    std::shared_ptr<IChatEngine> engine;
    std::shared_ptr<IModelManager> manager;
    std::shared_ptr<ILambdaStudioStore> catalog;
    std::shared_ptr<void> lifecycle;

    LambdaStudioRuntime(
        std::shared_ptr<IChatEngine> engineIn,
        std::shared_ptr<IModelManager> managerIn,
        std::shared_ptr<ILambdaStudioStore> catalogIn,
        std::shared_ptr<void> lifecycleIn = nullptr
    )
        : engine(std::move(engineIn)),
          manager(std::move(managerIn)),
          catalog(std::move(catalogIn)),
          lifecycle(std::move(lifecycleIn)) {
        if (!engine || !manager || !catalog) {
            throw std::invalid_argument("LambdaStudioRuntime requires engine, manager, and catalog");
        }
    }
};

struct LambdaStudioRuntimeDeps {
    std::shared_ptr<IChatEngine> engine;
    std::shared_ptr<IModelManager> manager;
    std::shared_ptr<ILambdaStudioStore> catalog;
    std::shared_ptr<void> lifecycle;
};

struct LambdaStudioRuntimeFactory {
    using PostModelEvent = std::function<void(lambda_studio_backend::ModelManagerEvent)>;
    using MakeEngine = std::function<std::shared_ptr<IChatEngine>()>;
    using MakeManager =
        std::function<std::shared_ptr<IModelManager>(std::shared_ptr<IChatEngine>, PostModelEvent)>;
    using MakeCatalog = std::function<std::shared_ptr<ILambdaStudioStore>()>;
    using MakeLifecycle = std::function<std::shared_ptr<void>()>;

    PostModelEvent postModelEvent;
    MakeEngine makeEngine;
    MakeManager makeManager;
    MakeCatalog makeCatalog;
    MakeLifecycle makeLifecycle;
};

inline std::shared_ptr<LambdaStudioRuntime> makeLambdaStudioRuntime(LambdaStudioRuntimeDeps deps) {
    return std::make_shared<LambdaStudioRuntime>(
        std::move(deps.engine),
        std::move(deps.manager),
        std::move(deps.catalog),
        std::move(deps.lifecycle)
    );
}

inline std::shared_ptr<LambdaStudioRuntime> makeLambdaStudioRuntime(LambdaStudioRuntimeFactory factory) {
    if (!factory.postModelEvent) {
        throw std::invalid_argument("LambdaStudioRuntimeFactory.postModelEvent must be set");
    }
    if (!factory.makeEngine || !factory.makeManager || !factory.makeCatalog) {
        throw std::invalid_argument("LambdaStudioRuntimeFactory has unset constructor callback");
    }

    auto engine = factory.makeEngine();
    if (!engine) {
        throw std::runtime_error("LambdaStudioRuntimeFactory.makeEngine returned null");
    }

    auto manager = factory.makeManager(engine, std::move(factory.postModelEvent));
    if (!manager) {
        throw std::runtime_error("LambdaStudioRuntimeFactory.makeManager returned null");
    }

    auto catalog = factory.makeCatalog();
    if (!catalog) {
        throw std::runtime_error("LambdaStudioRuntimeFactory.makeCatalog returned null");
    }

    std::shared_ptr<void> lifecycle;
    if (factory.makeLifecycle) {
        lifecycle = factory.makeLifecycle();
    }

    return makeLambdaStudioRuntime(LambdaStudioRuntimeDeps {
        .engine = std::move(engine),
        .manager = std::move(manager),
        .catalog = std::move(catalog),
        .lifecycle = std::move(lifecycle),
    });
}

LambdaStudioRuntimeFactory makeDefaultLambdaStudioRuntimeFactory(
    LambdaStudioRuntimeFactory::PostModelEvent postModelEvent
);

} // namespace lambda
