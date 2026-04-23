#pragma once

#include "AppState.hpp"
#include "Types.hpp"

namespace lambda {

inline LocalModel toLocalModel(lambda_studio_backend::LocalModelInfo const &model) {
    LocalModel local;
    local.path = model.path;
    local.repo = model.repo;
    local.tag = model.tag;
    local.sizeBytes = model.sizeBytes;
    local.name = model.displayName();
    return local;
}

inline RemoteModel toRemoteModel(lambda_studio_backend::HfModelInfo const &model) {
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

inline RemoteModelFile toRemoteModelFile(lambda_studio_backend::HfFileInfo const &file) {
    RemoteModelFile remoteFile;
    remoteFile.repoId = file.repoId;
    remoteFile.path = file.path;
    remoteFile.localPath = file.localPath;
    remoteFile.sizeBytes = file.sizeBytes;
    remoteFile.cached = file.cached;
    return remoteFile;
}

inline RemoteRepoDetail toRemoteRepoDetail(lambda_studio_backend::HfRepoDetailInfo const &detail) {
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

inline lambda_studio_backend::ChatMessage::Role toBackendRole(ChatRole role) {
    switch (role) {
    case ChatRole::User:
        return lambda_studio_backend::ChatMessage::Role::User;
    case ChatRole::Reasoning:
        return lambda_studio_backend::ChatMessage::Role::Reasoning;
    case ChatRole::Assistant:
        return lambda_studio_backend::ChatMessage::Role::Assistant;
    case ChatRole::Tool:
        return lambda_studio_backend::ChatMessage::Role::Tool;
    }
    return lambda_studio_backend::ChatMessage::Role::Assistant;
}

inline ChatToolCall toChatToolCall(lambda_studio_backend::ToolCall const &toolCall) {
    return ChatToolCall {
        .id = toolCall.id,
        .name = toolCall.name,
        .arguments = toolCall.arguments,
    };
}

inline lambda_studio_backend::ToolCall toBackendToolCall(ChatToolCall const &toolCall) {
    return lambda_studio_backend::ToolCall {
        .id = toolCall.id,
        .name = toolCall.name,
        .arguments = toolCall.arguments,
    };
}

inline ToolMessageState toToolMessageState(lambda_studio_backend::ToolExecutionState state) {
    switch (state) {
    case lambda_studio_backend::ToolExecutionState::PendingApproval:
        return ToolMessageState::PendingApproval;
    case lambda_studio_backend::ToolExecutionState::Running:
        return ToolMessageState::Running;
    case lambda_studio_backend::ToolExecutionState::Completed:
        return ToolMessageState::Completed;
    case lambda_studio_backend::ToolExecutionState::Denied:
        return ToolMessageState::Denied;
    case lambda_studio_backend::ToolExecutionState::Failed:
        return ToolMessageState::Failed;
    case lambda_studio_backend::ToolExecutionState::None:
        break;
    }
    return ToolMessageState::None;
}

inline lambda_studio_backend::ToolExecutionState toBackendToolExecutionState(ToolMessageState state) {
    switch (state) {
    case ToolMessageState::PendingApproval:
        return lambda_studio_backend::ToolExecutionState::PendingApproval;
    case ToolMessageState::Running:
        return lambda_studio_backend::ToolExecutionState::Running;
    case ToolMessageState::Completed:
        return lambda_studio_backend::ToolExecutionState::Completed;
    case ToolMessageState::Denied:
        return lambda_studio_backend::ToolExecutionState::Denied;
    case ToolMessageState::Failed:
        return lambda_studio_backend::ToolExecutionState::Failed;
    case ToolMessageState::None:
        break;
    }
    return lambda_studio_backend::ToolExecutionState::None;
}

} // namespace lambda
