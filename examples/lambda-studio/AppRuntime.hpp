#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

#include "Interfaces.hpp"

namespace lambda {

struct AppRuntime {
    std::shared_ptr<IChatEngine> engine;
    std::shared_ptr<IModelManager> manager;
    std::shared_ptr<IStore> catalog;
    std::shared_ptr<void> lifecycle;

    AppRuntime(
        std::shared_ptr<IChatEngine> engineIn,
        std::shared_ptr<IModelManager> managerIn,
        std::shared_ptr<IStore> catalogIn,
        std::shared_ptr<void> lifecycleIn = nullptr
    )
        : engine(std::move(engineIn)),
          manager(std::move(managerIn)),
          catalog(std::move(catalogIn)),
          lifecycle(std::move(lifecycleIn)) {
        if (!engine || !manager || !catalog) {
            throw std::invalid_argument("AppRuntime requires engine, manager, and catalog");
        }
    }
};

struct AppRuntimeDeps {
    std::shared_ptr<IChatEngine> engine;
    std::shared_ptr<IModelManager> manager;
    std::shared_ptr<IStore> catalog;
    std::shared_ptr<void> lifecycle;
};

struct AppRuntimeFactory {
    using PostModelEvent = std::function<void(lambda_studio_backend::ModelManagerEvent)>;
    using MakeEngine = std::function<std::shared_ptr<IChatEngine>()>;
    using MakeManager =
        std::function<std::shared_ptr<IModelManager>(std::shared_ptr<IChatEngine>, PostModelEvent)>;
    using MakeCatalog = std::function<std::shared_ptr<IStore>()>;
    using MakeLifecycle = std::function<std::shared_ptr<void>()>;

    PostModelEvent postModelEvent;
    MakeEngine makeEngine;
    MakeManager makeManager;
    MakeCatalog makeCatalog;
    MakeLifecycle makeLifecycle;
};

inline std::shared_ptr<AppRuntime> makeAppRuntime(AppRuntimeDeps deps) {
    return std::make_shared<AppRuntime>(
        std::move(deps.engine),
        std::move(deps.manager),
        std::move(deps.catalog),
        std::move(deps.lifecycle)
    );
}

inline std::shared_ptr<AppRuntime> makeAppRuntime(AppRuntimeFactory factory) {
    if (!factory.postModelEvent) {
        throw std::invalid_argument("AppRuntimeFactory.postModelEvent must be set");
    }
    if (!factory.makeEngine || !factory.makeManager || !factory.makeCatalog) {
        throw std::invalid_argument("AppRuntimeFactory has unset constructor callback");
    }

    auto engine = factory.makeEngine();
    if (!engine) {
        throw std::runtime_error("AppRuntimeFactory.makeEngine returned null");
    }

    auto manager = factory.makeManager(engine, std::move(factory.postModelEvent));
    if (!manager) {
        throw std::runtime_error("AppRuntimeFactory.makeManager returned null");
    }

    auto catalog = factory.makeCatalog();
    if (!catalog) {
        throw std::runtime_error("AppRuntimeFactory.makeCatalog returned null");
    }

    std::shared_ptr<void> lifecycle;
    if (factory.makeLifecycle) {
        lifecycle = factory.makeLifecycle();
    }

    return makeAppRuntime(AppRuntimeDeps {
        .engine = std::move(engine),
        .manager = std::move(manager),
        .catalog = std::move(catalog),
        .lifecycle = std::move(lifecycle),
    });
}

AppRuntimeFactory makeDefaultAppRuntimeFactory(
    AppRuntimeFactory::PostModelEvent postModelEvent
);

} // namespace lambda
