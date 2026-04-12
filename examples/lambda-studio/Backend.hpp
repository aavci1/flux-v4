#pragma once

#include <Flux/Core/EventQueue.hpp>

#include <memory>
#include <utility>

#include "AppState.hpp"
#include "LlamaEngine.hpp"
#include "ModelManager.hpp"

namespace lambda {

struct BackendServices {
    std::shared_ptr<lambda_backend::LlamaEngine> engine;
    std::shared_ptr<lambda_backend::ModelManager> manager;
};

inline std::shared_ptr<BackendServices> &backendSlot() {
    static std::shared_ptr<BackendServices> services;
    return services;
}

inline BackendServices &backend() {
    auto &slot = backendSlot();
    if (!slot) {
        slot = std::make_shared<BackendServices>();
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
