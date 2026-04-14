#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

#include "BackendInterfaces.hpp"
#include "LlamaEngine.hpp"
#include "ModelCatalogStore.hpp"
#include "ModelManager.hpp"

namespace lambda {

namespace detail {

class LlamaBackendLifecycle {
  public:
    LlamaBackendLifecycle() { refCount().fetch_add(1, std::memory_order_relaxed); }

    ~LlamaBackendLifecycle() {
        if (refCount().fetch_sub(1, std::memory_order_acq_rel) == 1) {
            llama_backend_free();
        }
    }

  private:
    static std::atomic<int> &refCount() {
        static std::atomic<int> count {0};
        return count;
    }
};

} // namespace detail

struct LambdaStudioRuntime {
    std::shared_ptr<IChatEngine> engine;
    std::shared_ptr<IModelManager> manager;
    std::shared_ptr<IModelCatalogStore> catalog;
    std::shared_ptr<void> lifecycle;

    LambdaStudioRuntime(
        std::shared_ptr<IChatEngine> engineIn,
        std::shared_ptr<IModelManager> managerIn,
        std::shared_ptr<IModelCatalogStore> catalogIn,
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
    std::shared_ptr<IModelCatalogStore> catalog;
    std::shared_ptr<void> lifecycle;
};

struct LambdaStudioRuntimeFactory {
    using PostModelEvent = std::function<void(lambda_backend::ModelManagerEvent)>;
    using MakeEngine = std::function<std::shared_ptr<IChatEngine>()>;
    using MakeManager =
        std::function<std::shared_ptr<IModelManager>(std::shared_ptr<IChatEngine>, PostModelEvent)>;
    using MakeCatalog = std::function<std::shared_ptr<IModelCatalogStore>()>;

    PostModelEvent postModelEvent;
    MakeEngine makeEngine = [] {
        return std::make_shared<lambda_backend::LlamaEngine>();
    };
    MakeManager makeManager = [](std::shared_ptr<IChatEngine> engine, PostModelEvent post) {
        return std::make_shared<lambda_backend::ModelManager>(std::move(engine), std::move(post));
    };
    MakeCatalog makeCatalog = [] {
        return std::make_shared<ModelCatalogStore>();
    };
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

    return makeLambdaStudioRuntime(LambdaStudioRuntimeDeps {
        .engine = std::move(engine),
        .manager = std::move(manager),
        .catalog = std::move(catalog),
        .lifecycle = std::make_shared<detail::LlamaBackendLifecycle>(),
    });
}

} // namespace lambda
