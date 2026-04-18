#include <doctest/doctest.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Scene/LocalId.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/StateStore.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../examples/lambda-studio/AppRuntime.hpp"

namespace {

class FakeChatEngine : public lambda::IChatEngine {
  public:
    bool load(lambda_studio_backend::LoadParams const &params) override {
        loaded_ = true;
        loadParams_ = params;
        modelPath_ = params.modelPath;
        return true;
    }

    lambda_studio_backend::LoadParams loadParams() const override { return loadParams_; }
    lambda_studio_backend::SessionParams sessionDefaults() const override { return sessionDefaults_; }
    lambda_studio_backend::GenerationParams generationDefaults() const override { return generationDefaults_; }

    bool isLoaded() const override { return loaded_; }
    std::string const &modelPath() const override { return modelPath_; }

    std::optional<lambda_studio_backend::GenerationParams> chatGenerationDefaults(std::string const &chatId) const override {
        auto it = chatGenerationDefaults_.find(chatId);
        return it == chatGenerationDefaults_.end() ? std::nullopt :
                                                     std::optional<lambda_studio_backend::GenerationParams>(it->second);
    }

    lambda_studio_backend::ApplyResult updateLoadParams(lambda_studio_backend::LoadParamsPatch const &) override {
        return {.scope = lambda_studio_backend::ApplyScope::AppliedImmediately, .message = ""};
    }
    lambda_studio_backend::ApplyResult updateSessionDefaults(lambda_studio_backend::SessionParamsPatch const &) override {
        return {.scope = lambda_studio_backend::ApplyScope::AppliedImmediately, .message = ""};
    }
    lambda_studio_backend::ApplyResult updateGenerationDefaults(lambda_studio_backend::GenerationParamsPatch const &) override {
        return {.scope = lambda_studio_backend::ApplyScope::AppliedImmediately, .message = ""};
    }
    lambda_studio_backend::ApplyResult updateChatGenerationParams(
        std::string const &chatId,
        lambda_studio_backend::GenerationParamsPatch const &
    ) override {
        chatGenerationDefaults_[chatId] = generationDefaults_;
        return {.scope = lambda_studio_backend::ApplyScope::AppliedImmediately, .message = ""};
    }

    void unload() override {
        loaded_ = false;
        modelPath_.clear();
    }

    void cancelChat(std::string const &) override { ++cancelChatCalls_; }
    void cancelAllGenerations() override { ++cancelAllCalls_; }
    void respondToToolApproval(std::string const &, std::uint64_t, std::string const &, bool) override {}

    void startChat(
        lambda_studio_backend::ChatGenerationRequest,
        std::function<void(lambda_studio_backend::LlmUiEvent)>
    ) override {
        ++startChatCalls_;
    }

    int startChatCalls() const { return startChatCalls_; }
    int cancelChatCalls() const { return cancelChatCalls_; }
    int cancelAllCalls() const { return cancelAllCalls_; }

  private:
    bool loaded_ = false;
    std::string modelPath_;
    lambda_studio_backend::LoadParams loadParams_ {};
    lambda_studio_backend::SessionParams sessionDefaults_ {};
    lambda_studio_backend::GenerationParams generationDefaults_ {};
    std::unordered_map<std::string, lambda_studio_backend::GenerationParams> chatGenerationDefaults_;
    int startChatCalls_ = 0;
    int cancelChatCalls_ = 0;
    int cancelAllCalls_ = 0;
};

class FakeModelManager : public lambda::IModelManager {
  public:
    explicit FakeModelManager(std::shared_ptr<lambda::IChatEngine> engine) : boundEngine_(std::move(engine)) {}

    std::uint64_t refreshLocalModels() override { return ++requestId_; }
    std::uint64_t searchHuggingFace(lambda_studio_backend::HfSearchRequest) override { return ++requestId_; }
    std::uint64_t listRepoFiles(std::string) override { return ++requestId_; }
    std::uint64_t fetchRepoDetail(std::string) override { return ++requestId_; }
    std::uint64_t inspectRepo(std::string) override { return ++requestId_; }
    std::uint64_t downloadModel(std::string, std::string) override { return ++requestId_; }
    std::uint64_t cancelDownload() override { return ++requestId_; }
    std::uint64_t loadModel(lambda_studio_backend::LoadParams) override { return ++requestId_; }
    std::uint64_t deleteModel(std::string, std::string) override { return ++requestId_; }
    void unloadModel() override {}

    std::weak_ptr<lambda::IChatEngine> boundEngine() const { return boundEngine_; }

  private:
    std::shared_ptr<lambda::IChatEngine> boundEngine_;
    std::uint64_t requestId_ = 0;
};

class FakeStore : public lambda::IStore {
  public:
    std::filesystem::path databasePath() const override { return {}; }
    void replaceSearchSnapshot(std::string const &, std::vector<lambda::RemoteModel> const &, std::string const &) override {}
    std::vector<lambda::RemoteModel> loadSearchResults(std::string const &) override { return {}; }
    std::vector<lambda::RemoteModel> searchCatalogModels(
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
    void deleteDownloadJob(std::string const &) override {}
    void deleteDownloadJobsForArtifact(std::string const &, std::string const &) override {}
    std::vector<lambda::DownloadJob> loadRecentDownloadJobs(std::size_t) override { return {}; }
    void markRunningDownloadJobsInterrupted(std::int64_t) override {}
    lambda::PersistedChatState loadPersistedChatState() override { return {}; }
    void upsertChatThreadMeta(
        std::string const &,
        std::string const &,
        std::int64_t,
        std::string const &,
        std::string const &,
        std::string const &,
        std::size_t,
        std::int64_t,
        std::int64_t
    ) override {}
    void replaceChatMessagesForThread(std::string const &, std::vector<lambda::ChatMessage> const &) override {}
    void deleteChatThread(std::string const &) override {}
    void updateSelectedChatId(std::string const &) override {}
    void replaceChatOrder(std::vector<std::string> const &) override {}
    void updateChatThreadGenerationDefaults(
        std::string const &,
        std::optional<lambda_studio_backend::GenerationParams> const &
    ) override {}
    std::optional<lambda_studio_backend::EngineConfigDefaults> loadEngineConfigDefaults() override {
        return std::nullopt;
    }
    void saveEngineConfigDefaults(lambda_studio_backend::EngineConfigDefaults const &) override {}
};

} // namespace

TEST_CASE("AppRuntime rejects missing dependencies") {
    auto engine = std::make_shared<FakeChatEngine>();
    auto manager = std::make_shared<FakeModelManager>(engine);
    auto catalog = std::make_shared<FakeStore>();

    CHECK_NOTHROW(lambda::makeAppRuntime(lambda::AppRuntimeDeps {
        .engine = engine,
        .manager = manager,
        .catalog = catalog,
    }));

    CHECK_THROWS_AS(lambda::makeAppRuntime(lambda::AppRuntimeDeps {
                        .engine = nullptr,
                        .manager = manager,
                        .catalog = catalog,
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(lambda::makeAppRuntime(lambda::AppRuntimeDeps {
                        .engine = engine,
                        .manager = nullptr,
                        .catalog = catalog,
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(lambda::makeAppRuntime(lambda::AppRuntimeDeps {
                        .engine = engine,
                        .manager = manager,
                        .catalog = nullptr,
                    }),
                    std::invalid_argument);
}

TEST_CASE("AppRuntimeFactory builds isolated runtimes and tears down lifecycle independently") {
    std::atomic<int> lifecycleDestroyCount {0};
    std::atomic<int> postCount {0};
    std::atomic<int> engineBuildCount {0};
    std::atomic<int> managerBuildCount {0};
    std::atomic<int> catalogBuildCount {0};

    lambda::AppRuntimeFactory factory {
        .postModelEvent =
            [&](lambda_studio_backend::ModelManagerEvent) {
                ++postCount;
            },
        .makeEngine =
            [&]() -> std::shared_ptr<lambda::IChatEngine> {
                ++engineBuildCount;
                return std::make_shared<FakeChatEngine>();
            },
        .makeManager =
            [&](std::shared_ptr<lambda::IChatEngine> engine, lambda::AppRuntimeFactory::PostModelEvent) -> std::shared_ptr<lambda::IModelManager> {
                ++managerBuildCount;
                return std::make_shared<FakeModelManager>(std::move(engine));
            },
        .makeCatalog =
            [&]() -> std::shared_ptr<lambda::IStore> {
                ++catalogBuildCount;
                return std::make_shared<FakeStore>();
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

    auto runtimeA = lambda::makeAppRuntime(factory);
    auto runtimeB = lambda::makeAppRuntime(factory);

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

TEST_CASE("Composite-observed signals schedule the next reactive frame") {
    using namespace std::chrono_literals;

    flux::Application app;
    flux::StateStore store;
    flux::Signal<int> signal {0};
    flux::ComponentKey key {flux::LocalId::fromIndex(0)};

    (void)signal.observeComposite(store, key);

    std::atomic<int> frameCount {0};
    auto handle = app.onNextFrameNeeded([&] {
        ++frameCount;
        app.quit();
    });

    std::jthread failsafe([&] {
        std::this_thread::sleep_for(150ms);
        app.quit();
    });

    signal.set(1);
    int const exitCode = app.exec();
    app.unobserveNextFrame(handle);

    CHECK(exitCode == 0);
    CHECK(frameCount.load() == 1);
}
