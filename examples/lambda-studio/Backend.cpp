#include "Backend.hpp"

#include <atomic>

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

LambdaStudioRuntimeFactory makeDefaultLambdaStudioRuntimeFactory(
    LambdaStudioRuntimeFactory::PostModelEvent postModelEvent
) {
    return LambdaStudioRuntimeFactory {
        .postModelEvent = std::move(postModelEvent),
        .makeEngine = [] {
            return std::make_shared<lambda_backend::LlamaEngine>();
        },
        .makeManager = [](std::shared_ptr<IChatEngine> engine, LambdaStudioRuntimeFactory::PostModelEvent post) {
            return std::make_shared<lambda_backend::ModelManager>(std::move(engine), std::move(post));
        },
        .makeCatalog = [] {
            return std::make_shared<ModelCatalogStore>();
        },
        .makeLifecycle = [] {
            return std::make_shared<detail::LlamaBackendLifecycle>();
        },
    };
}

} // namespace lambda
