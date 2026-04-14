#include <doctest/doctest.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../examples/lambda-studio/Backend.hpp"

namespace {

class FakeChatEngine : public lambda::IChatEngine {
  public:
    bool load(std::string const &modelPath, int, uint32_t) override {
        loaded_ = true;
        modelPath_ = modelPath;
        return true;
    }

    bool isLoaded() const override { return loaded_; }
    std::string const &modelPath() const override { return modelPath_; }

    lambda_backend::SamplingParams samplingParams() const override { return sampling_; }
    void setSamplingParams(lambda_backend::SamplingParams const &params) override { sampling_ = params; }

    void unload() override {
        loaded_ = false;
        modelPath_.clear();
    }

    void cancelGeneration() override { ++cancelCalls_; }

    void startChat(
        std::vector<lambda_backend::ChatMessage>,
        std::string,
        std::uint64_t,
        std::function<void(lambda_backend::LlmUiEvent)>
    ) override {
        ++startChatCalls_;
    }

    int startChatCalls() const { return startChatCalls_; }
    int cancelCalls() const { return cancelCalls_; }

  private:
    bool loaded_ = false;
    std::string modelPath_;
    lambda_backend::SamplingParams sampling_ {};
    int startChatCalls_ = 0;
    int cancelCalls_ = 0;
};

class FakeModelManager : public lambda::IModelManager {
  public:
    explicit FakeModelManager(std::shared_ptr<lambda::IChatEngine> engine) : boundEngine_(std::move(engine)) {}

    std::uint64_t refreshLocalModels() override { return ++requestId_; }
    std::uint64_t searchHuggingFace(lambda_backend::HfSearchRequest) override { return ++requestId_; }
    std::uint64_t listRepoFiles(std::string) override { return ++requestId_; }
    std::uint64_t fetchRepoDetail(std::string) override { return ++requestId_; }
    std::uint64_t inspectRepo(std::string) override { return ++requestId_; }
    std::uint64_t downloadModel(std::string, std::string) override { return ++requestId_; }
    std::uint64_t loadModel(std::string, int) override { return ++requestId_; }
    std::uint64_t deleteModel(std::string, std::string) override { return ++requestId_; }
    void unloadModel() override {}

    std::weak_ptr<lambda::IChatEngine> boundEngine() const { return boundEngine_; }

  private:
    std::shared_ptr<lambda::IChatEngine> boundEngine_;
    std::uint64_t requestId_ = 0;
};

class FakeModelCatalogStore : public lambda::IModelCatalogStore {
  public:
    std::filesystem::path databasePath() const override { return {}; }
    void replaceSearchSnapshot(std::string const &, std::vector<lambda::RemoteModel> const &, std::string const &) override {}
    std::vector<lambda::RemoteModel> loadSearchResults(std::string const &) override { return {}; }
    std::vector<lambda::RemoteModel> searchCatalogModels(
        std::string const &,
        std::string const &,
        lambda::RemoteModelSort,
        lambda::RemoteModelVisibilityFilter,
        std::size_t
    ) override {
        return {};
    }
    void replaceRepoFilesSnapshot(std::string const &, std::vector<lambda::RemoteModelFile> const &, std::string const &) override {}
    std::vector<lambda::RemoteModelFile> loadRepoFiles(std::string const &) override { return {}; }
    void replaceRepoDetailSnapshot(lambda::RemoteRepoDetail const &, std::string const &) override {}
    std::optional<lambda::RemoteRepoDetail> loadRepoDetail(std::string const &) override { return std::nullopt; }
    void replaceLocalModelInstances(std::vector<lambda::LocalModel> const &) override {}
    std::vector<lambda::LocalModel> loadLocalModelInstances() override { return {}; }
    void startDownloadJob(lambda::DownloadJob const &) override {}
    void finishDownloadJob(std::string const &, std::string const &, std::int64_t) override {}
    void failDownloadJob(std::string const &, std::string const &, std::int64_t) override {}
    std::vector<lambda::DownloadJob> loadRecentDownloadJobs(std::size_t) override { return {}; }
    void markRunningDownloadJobsInterrupted(std::int64_t) override {}
    lambda::PersistedChatState loadPersistedChatState() override { return {}; }
    void upsertChatThreadMeta(
        std::string const &,
        std::string const &,
        std::int64_t,
        std::string const &,
        std::string const &,
        std::int64_t
    ) override {}
    void replaceChatMessagesForThread(std::string const &, std::vector<lambda::ChatMessage> const &) override {}
    void deleteChatThread(std::string const &) override {}
    void updateSelectedChatId(std::string const &) override {}
    void replaceChatOrder(std::vector<std::string> const &) override {}
};

} // namespace

TEST_CASE("LambdaStudioRuntime rejects missing dependencies") {
    auto engine = std::make_shared<FakeChatEngine>();
    auto manager = std::make_shared<FakeModelManager>(engine);
    auto catalog = std::make_shared<FakeModelCatalogStore>();

    CHECK_NOTHROW(lambda::makeLambdaStudioRuntime(lambda::LambdaStudioRuntimeDeps {
        .engine = engine,
        .manager = manager,
        .catalog = catalog,
    }));

    CHECK_THROWS_AS(lambda::makeLambdaStudioRuntime(lambda::LambdaStudioRuntimeDeps {
                        .engine = nullptr,
                        .manager = manager,
                        .catalog = catalog,
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(lambda::makeLambdaStudioRuntime(lambda::LambdaStudioRuntimeDeps {
                        .engine = engine,
                        .manager = nullptr,
                        .catalog = catalog,
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(lambda::makeLambdaStudioRuntime(lambda::LambdaStudioRuntimeDeps {
                        .engine = engine,
                        .manager = manager,
                        .catalog = nullptr,
                    }),
                    std::invalid_argument);
}

TEST_CASE("LambdaStudioRuntimeFactory builds isolated runtimes and tears down lifecycle independently") {
    std::atomic<int> lifecycleDestroyCount {0};
    std::atomic<int> postCount {0};
    std::atomic<int> engineBuildCount {0};
    std::atomic<int> managerBuildCount {0};
    std::atomic<int> catalogBuildCount {0};

    lambda::LambdaStudioRuntimeFactory factory {
        .postModelEvent =
            [&](lambda_backend::ModelManagerEvent) {
                ++postCount;
            },
        .makeEngine =
            [&]() -> std::shared_ptr<lambda::IChatEngine> {
                ++engineBuildCount;
                return std::make_shared<FakeChatEngine>();
            },
        .makeManager =
            [&](std::shared_ptr<lambda::IChatEngine> engine, lambda::LambdaStudioRuntimeFactory::PostModelEvent) -> std::shared_ptr<lambda::IModelManager> {
                ++managerBuildCount;
                return std::make_shared<FakeModelManager>(std::move(engine));
            },
        .makeCatalog =
            [&]() -> std::shared_ptr<lambda::IModelCatalogStore> {
                ++catalogBuildCount;
                return std::make_shared<FakeModelCatalogStore>();
            },
        .makeLifecycle =
            [&]() -> std::shared_ptr<void> {
                return std::shared_ptr<void>(
                    new int(1),
                    [&](void *p) {
                        delete static_cast<int *>(p);
                        ++lifecycleDestroyCount;
                    }
                );
            },
    };

    auto runtimeA = lambda::makeLambdaStudioRuntime(factory);
    auto runtimeB = lambda::makeLambdaStudioRuntime(factory);

    REQUIRE(runtimeA != nullptr);
    REQUIRE(runtimeB != nullptr);
    CHECK(runtimeA != runtimeB);
    CHECK(runtimeA->engine != runtimeB->engine);
    CHECK(runtimeA->manager != runtimeB->manager);
    CHECK(runtimeA->catalog != runtimeB->catalog);

    auto managerA = std::dynamic_pointer_cast<FakeModelManager>(runtimeA->manager);
    auto managerB = std::dynamic_pointer_cast<FakeModelManager>(runtimeB->manager);
    REQUIRE(managerA != nullptr);
    REQUIRE(managerB != nullptr);
    CHECK(managerA->boundEngine().lock() == runtimeA->engine);
    CHECK(managerB->boundEngine().lock() == runtimeB->engine);
    CHECK(managerA->boundEngine().lock() != managerB->boundEngine().lock());

    CHECK(engineBuildCount.load() == 2);
    CHECK(managerBuildCount.load() == 2);
    CHECK(catalogBuildCount.load() == 2);
    CHECK(postCount.load() == 0);

    runtimeA.reset();
    CHECK(lifecycleDestroyCount.load() == 1);
    runtimeB.reset();
    CHECK(lifecycleDestroyCount.load() == 2);
}
