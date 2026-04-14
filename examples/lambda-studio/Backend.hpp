#pragma once

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>

#include <atomic>
#include <memory>
#include <utility>

#include "BackendInterfaces.hpp"
#include "LlamaEngine.hpp"
#include "ModelCatalogStore.hpp"
#include "ModelManager.hpp"

namespace lambda {

struct LambdaStudioRuntime {
    std::shared_ptr<IChatEngine> engine;
    std::shared_ptr<IModelManager> manager;
    std::shared_ptr<IModelCatalogStore> catalog;

    LambdaStudioRuntime() { runtimeRefCount().fetch_add(1, std::memory_order_relaxed); }

    ~LambdaStudioRuntime() {
        if (runtimeRefCount().fetch_sub(1, std::memory_order_acq_rel) == 1) {
            llama_backend_free();
        }
    }

  private:
    static std::atomic<int> &runtimeRefCount() {
        static std::atomic<int> count {0};
        return count;
    }
};

inline std::shared_ptr<LambdaStudioRuntime> makeLambdaStudioRuntime() {
    auto runtime = std::make_shared<LambdaStudioRuntime>();
    auto engine = std::make_shared<lambda_backend::LlamaEngine>();
    auto manager = std::make_shared<lambda_backend::ModelManager>(
        engine,
        [](lambda_backend::ModelManagerEvent ev) {
            flux::Application::instance().eventQueue().post(std::move(ev));
        }
    );
    auto catalog = std::make_shared<ModelCatalogStore>();

    runtime->engine = std::move(engine);
    runtime->manager = std::move(manager);
    runtime->catalog = std::move(catalog);
    return runtime;
}

} // namespace lambda
