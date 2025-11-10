#include "pch.h"
#include "SnapshotManager.hpp"
#include "EditorState.hpp"
#include "Scene/SceneManager.hpp"
#include "Serialization/Serializer.hpp"
#include "Logging.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

SnapshotManager& SnapshotManager::GetInstance() {
    static SnapshotManager instance;
    return instance;
}

void SnapshotManager::TakeSnapshot(const std::string& description) {
    if (!snapshotEnabled) {
        return; // Snapshots disabled
    }

    // Capture current state
    Snapshot snapshot = CaptureCurrentState(description);

    // Add to undo stack
    undoStack.push_back(std::move(snapshot));

    // Maintain max size - remove oldest if exceeded
    if (undoStack.size() > MAX_UNDO_SNAPSHOTS) {
        undoStack.pop_front();
    }

    // Clear redo stack as taking a new snapshot invalidates forward history
    while (!redoStack.empty()) {
        redoStack.pop();
    }

    ENGINE_LOG_INFO("[SnapshotManager] Snapshot taken: " + description +
                    " (Undo: " + std::to_string(undoStack.size()) + ")");
}

bool SnapshotManager::Undo() {
    if (!CanUndo()) {
        ENGINE_LOG_WARN("[SnapshotManager] Cannot undo - no undo history available");
        return false;
    }

    // Capture current state before undoing (for redo)
    Snapshot currentState = CaptureCurrentState("Redo point");

    // Get the previous state from undo stack
    Snapshot previousState = std::move(undoStack.back());
    undoStack.pop_back();

    // Push current state to redo stack
    redoStack.push(std::move(currentState));

    // Restore the previous state
    RestoreSnapshot(previousState);

    ENGINE_LOG_INFO("[SnapshotManager] Undo performed (Undo: " +
                    std::to_string(undoStack.size()) + ", Redo: " +
                    std::to_string(redoStack.size()) + ")");
    return true;
}

bool SnapshotManager::Redo() {
    if (!CanRedo()) {
        ENGINE_LOG_WARN("[SnapshotManager] Cannot redo - no redo history available");
        return false;
    }

    // Capture current state before redoing (for undo)
    Snapshot currentState = CaptureCurrentState("Undo point");

    // Get the next state from redo stack
    Snapshot nextState = std::move(redoStack.top());
    redoStack.pop();

    // Push current state to undo stack
    undoStack.push_back(std::move(currentState));

    // Maintain max size
    if (undoStack.size() > MAX_UNDO_SNAPSHOTS) {
        undoStack.pop_front();
    }

    // Restore the next state
    RestoreSnapshot(nextState);

    ENGINE_LOG_INFO("[SnapshotManager] Redo performed (Undo: " +
                    std::to_string(undoStack.size()) + ", Redo: " +
                    std::to_string(redoStack.size()) + ")");
    return true;
}

bool SnapshotManager::CanUndo() const {
    return !undoStack.empty();
}

bool SnapshotManager::CanRedo() const {
    return !redoStack.empty();
}

void SnapshotManager::Clear() {
    undoStack.clear();
    while (!redoStack.empty()) {
        redoStack.pop();
    }
    ENGINE_LOG_INFO("[SnapshotManager] History cleared");
}

std::string SnapshotManager::SerializeCurrentScene() const {
    // Use a temporary file to serialize the scene
    std::string tempPath = ".snapshot_temp.scene";

    try {
        // Serialize scene to temp file
        Serializer::SerializeScene(tempPath);

        // Read the file contents into a string
        std::ifstream file(tempPath, std::ios::binary);
        if (!file.is_open()) {
            ENGINE_LOG_ERROR("[SnapshotManager] Failed to open temp file for reading");
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        // Clean up temp file
        std::filesystem::remove(tempPath);

        return buffer.str();
    }
    catch (const std::exception& e) {
        ENGINE_LOG_ERROR("[SnapshotManager] Exception during scene serialization: " +
                        std::string(e.what()));
        // Clean up temp file if it exists
        if (std::filesystem::exists(tempPath)) {
            std::filesystem::remove(tempPath);
        }
        return "";
    }
}

bool SnapshotManager::DeserializeScene(const std::string& sceneData) {
    if (sceneData.empty()) {
        ENGINE_LOG_ERROR("[SnapshotManager] Cannot deserialize empty scene data");
        return false;
    }

    // Use a temporary file to deserialize the scene
    std::string tempPath = ".snapshot_restore.scene";

    try {
        // Write the scene data to a temp file
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            ENGINE_LOG_ERROR("[SnapshotManager] Failed to open temp file for writing");
            return false;
        }

        file << sceneData;
        file.close();

        // Get current scene path to restore it after deserialization
        std::string currentScenePath = SceneManager::GetInstance().GetCurrentScenePath();

        // Deserialize from temp file using ReloadScene which preserves the scene path
        Serializer::ReloadScene(tempPath, currentScenePath);

        // Clean up temp file
        std::filesystem::remove(tempPath);

        return true;
    }
    catch (const std::exception& e) {
        ENGINE_LOG_ERROR("[SnapshotManager] Exception during scene deserialization: " +
                        std::string(e.what()));
        // Clean up temp file if it exists
        if (std::filesystem::exists(tempPath)) {
            std::filesystem::remove(tempPath);
        }
        return false;
    }
}

SnapshotManager::Snapshot SnapshotManager::CaptureCurrentState(const std::string& description) const {
    // Serialize current scene
    std::string sceneData = SerializeCurrentScene();

    // Get currently selected entity
    Entity selectedEntity = EditorState::GetInstance().GetSelectedEntity();

    return Snapshot(sceneData, selectedEntity, description);
}

void SnapshotManager::RestoreSnapshot(const Snapshot& snapshot) {
    // Deserialize scene data
    if (!DeserializeScene(snapshot.sceneData)) {
        ENGINE_LOG_ERROR("[SnapshotManager] Failed to restore snapshot");
        return;
    }

    // Restore selected entity
    EditorState::GetInstance().SetSelectedEntity(snapshot.selectedEntity);

    ENGINE_LOG_INFO("[SnapshotManager] Snapshot restored: " + snapshot.description);
}
