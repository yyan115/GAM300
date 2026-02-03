#pragma once

#include <string>
#include <deque>
#include <stack>
#include <cstdint>
#include <chrono>
#include <ECS/Entity.hpp>

/**
 * @brief Manages undo/redo functionality for the scene editor.
 *
 * The SnapshotManager captures scene states as serialized JSON snapshots
 * and provides undo/redo operations. It maintains:
 * - Up to 20 undo snapshots in a circular buffer
 * - A redo stack for forward navigation
 * - Selected entity state with each snapshot
 *
 * Usage:
 * - Call TakeSnapshot() after any significant scene modification
 * - Call Undo() to restore previous state (Ctrl+Z)
 * - Call Redo() to restore next state (Ctrl+Y)
 * - Call Clear() to reset history (e.g., when loading a new scene)
 */
class SnapshotManager {
public:
    /**
     * @brief Get the singleton instance.
     */
    static SnapshotManager& GetInstance();

    /**
     * @brief Captures the current scene state as a snapshot.
     *
     * Serializes the entire scene to JSON and stores it in the undo history.
     * Clears the redo stack as taking a new snapshot invalidates forward history.
     * If the undo history exceeds MAX_UNDO_SNAPSHOTS (20), the oldest snapshot is removed.
     *
     * @param description Optional description of the action (for debugging/UI)
     */
    void TakeSnapshot(const std::string& description = "");

    /**
     * @brief Undoes the last action by restoring the previous snapshot.
     *
     * Moves the current state to the redo stack and restores the previous
     * scene state from the undo history.
     *
     * @return true if undo was successful, false if no undo history exists
     */
    bool Undo();

    /**
     * @brief Redoes the last undone action.
     *
     * Restores the next state from the redo stack.
     *
     * @return true if redo was successful, false if no redo history exists
     */
    bool Redo();

    /**
     * @brief Checks if undo operation is available.
     *
     * @return true if there are snapshots in the undo history
     */
    bool CanUndo() const;

    /**
     * @brief Checks if redo operation is available.
     *
     * @return true if there are snapshots in the redo history
     */
    bool CanRedo() const;

    /**
     * @brief Clears all undo/redo history.
     *
     * Should be called when loading a new scene or when history should be reset.
     */
    void Clear();

    /**
     * @brief Gets the number of available undo operations.
     *
     * @return Number of snapshots in undo history
     */
    size_t GetUndoCount() const { return undoStack.size(); }

    /**
     * @brief Gets the number of available redo operations.
     *
     * @return Number of snapshots in redo history
     */
    size_t GetRedoCount() const { return redoStack.size(); }

    /**
     * @brief Enable or disable automatic snapshot capture.
     *
     * When disabled, snapshots must be taken manually via TakeSnapshot().
     * Useful for batch operations where you want to capture only the final state.
     *
     * @param enabled true to enable automatic snapshots, false to disable
     */
    void SetSnapshotEnabled(bool enabled) { snapshotEnabled = enabled; }

    /**
     * @brief Check if automatic snapshot capture is enabled.
     *
     * @return true if snapshots are enabled
     */
    bool IsSnapshotEnabled() const { return snapshotEnabled; }

private:
    SnapshotManager() = default;
    ~SnapshotManager() = default;
    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;

    /**
     * @brief Represents a single snapshot of the scene state.
     */
    struct Snapshot {
        std::string sceneData;          // Serialized scene JSON
        Entity selectedEntity;          // Selected entity at snapshot time
        std::string description;        // Description of the action (for debugging)
        std::chrono::system_clock::time_point timestamp; // When snapshot was taken

        Snapshot(const std::string& data, Entity entity, const std::string& desc)
            : sceneData(data)
            , selectedEntity(entity)
            , description(desc)
            , timestamp(std::chrono::system_clock::now())
        {}
    };

    /**
     * @brief Serializes the current scene to a JSON string.
     *
     * @return JSON string containing the entire scene state
     */
    std::string SerializeCurrentScene() const;

    /**
     * @brief Deserializes a scene from a JSON string.
     *
     * @param sceneData JSON string containing scene state
     * @return true if deserialization was successful
     */
    bool DeserializeScene(const std::string& sceneData);

    /**
     * @brief Captures the current scene state and returns a Snapshot.
     *
     * @param description Description of the action
     * @return Snapshot containing current scene state
     */
    Snapshot CaptureCurrentState(const std::string& description) const;

    /**
     * @brief Restores a snapshot to the current scene.
     *
     * @param snapshot The snapshot to restore
     */
    void RestoreSnapshot(const Snapshot& snapshot);

    static constexpr size_t MAX_UNDO_SNAPSHOTS = 20;  // Maximum number of undo snapshots

    std::deque<Snapshot> undoStack;     // Undo history (FIFO with max size)
    std::stack<Snapshot> redoStack;     // Redo history (LIFO)
    bool snapshotEnabled = false;        // Whether automatic snapshots are enabled
};
