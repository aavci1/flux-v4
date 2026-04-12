#pragma once

#include <Flux/Core/EventQueue.hpp>

#include <memory>
#include <utility>

#include "AppState.hpp"
#include "LlamaEngine.hpp"
#include "ModelCatalogStore.hpp"
#include "ModelManager.hpp"

namespace lambda {

struct BackendServices {
    std::shared_ptr<lambda_backend::LlamaEngine> engine;
    std::shared_ptr<lambda_backend::ModelManager> manager;
    std::shared_ptr<ModelCatalogStore> catalog;
};

inline std::shared_ptr<BackendServices> &backendSlot() {
    static std::shared_ptr<BackendServices> services;
    return services;
}

inline BackendServices &backend() {
    auto &slot = backendSlot();
    if (!slot) {
        slot = std::make_shared<BackendServices>();
        slot->catalog = std::make_shared<ModelCatalogStore>();
        slot->engine = std::make_shared<lambda_backend::LlamaEngine>();
        slot->manager = std::make_shared<lambda_backend::ModelManager>(
            slot->engine,
            [](lambda_backend::ModelManagerEvent ev) {
                flux::Application::instance().eventQueue().post(std::move(ev));
            }
        );
    }
    return *slot;
}

inline void shutdownBackend() {
    auto &slot = backendSlot();
    if (!slot) {
        return;
    }
    slot->manager.reset();
    slot->engine.reset();
    slot->catalog.reset();
    llama_backend_free();
    slot.reset();
}

inline LocalModel toLocalModel(lambda_backend::LocalModelInfo const &model) {
    LocalModel local;
    local.path = model.path;
    local.repo = model.repo;
    local.tag = model.tag;
    local.sizeBytes = model.sizeBytes;
    local.name = model.displayName();
    return local;
}

inline RemoteModel toRemoteModel(lambda_backend::HfModelInfo const &model) {
    RemoteModel remote;
    remote.id = model.id;
    remote.author = model.author;
    remote.libraryName = model.libraryName;
    remote.pipelineTag = model.pipelineTag;
    remote.createdAt = model.createdAt;
    remote.lastModified = model.lastModified;
    remote.tags = model.tags;
    remote.downloads = model.downloads;
    remote.downloadsAllTime = model.downloadsAllTime;
    remote.likes = model.likes;
    remote.usedStorage = model.usedStorage;
    remote.gated = model.gated;
    remote.isPrivate = model.isPrivate;
    remote.disabled = model.disabled;
    return remote;
}

inline RemoteModelFile toRemoteModelFile(lambda_backend::HfFileInfo const &file) {
    RemoteModelFile remoteFile;
    remoteFile.repoId = file.repoId;
    remoteFile.path = file.path;
    remoteFile.localPath = file.localPath;
    remoteFile.sizeBytes = file.sizeBytes;
    remoteFile.cached = file.cached;
    return remoteFile;
}

inline RemoteRepoDetail toRemoteRepoDetail(lambda_backend::HfRepoDetailInfo const &detail) {
    RemoteRepoDetail remote;
    remote.id = detail.id;
    remote.author = detail.author;
    remote.sha = detail.sha;
    remote.libraryName = detail.libraryName;
    remote.pipelineTag = detail.pipelineTag;
    remote.license = detail.license;
    remote.summary = detail.summary;
    remote.readme = detail.readme;
    remote.createdAt = detail.createdAt;
    remote.lastModified = detail.lastModified;
    remote.tags = detail.tags;
    remote.languages = detail.languages;
    remote.baseModels = detail.baseModels;
    remote.downloads = detail.downloads;
    remote.downloadsAllTime = detail.downloadsAllTime;
    remote.likes = detail.likes;
    remote.usedStorage = detail.usedStorage;
    remote.gated = detail.gated;
    remote.isPrivate = detail.isPrivate;
    remote.disabled = detail.disabled;
    return remote;
}

inline lambda_backend::ChatMessage::Role toBackendRole(ChatRole role) {
    switch (role) {
    case ChatRole::User:
        return lambda_backend::ChatMessage::Role::User;
    case ChatRole::Reasoning:
        return lambda_backend::ChatMessage::Role::Reasoning;
    case ChatRole::Assistant:
        return lambda_backend::ChatMessage::Role::Assistant;
    }
    return lambda_backend::ChatMessage::Role::Assistant;
}

} // namespace lambda
