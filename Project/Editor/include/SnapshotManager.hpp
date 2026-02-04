/* Start Header ************************************************************************/
/*!
\file       SnapshotManager.hpp
\author     Claude (Rewrite)
\date       2026
\brief      Compatibility wrapper that redirects to the new UndoSystem.

            The old SnapshotManager serialized the ENTIRE SCENE on every edit,
            which caused:
            - 3-5 second lag on undo/redo (scene reload)
            - Lag when clicking on fields (scene serialization)
            - Animation/resource reloading

            This file now redirects to UndoSystem which uses instant commands.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

#include <string>
#include "UndoSystem.hpp"

/**
 * @brief Compatibility wrapper - redirects to UndoSystem
 *
 * Old code calling SnapshotManager will now use the new instant undo system.
 * TakeSnapshot() for simple property edits is now a no-op (the widgets handle it).
 */
class SnapshotManager {
public:
    static SnapshotManager& GetInstance();

    /**
     * @brief Legacy method - now a no-op for property edits
     *
     * Property edits are now handled automatically by UndoableWidgets.
     * This method does nothing for compatibility.
     */
    void TakeSnapshot(const std::string& description = "");

    /**
     * @brief Undo the last action (INSTANT - no scene reload)
     */
    bool Undo();

    /**
     * @brief Redo the last undone action (INSTANT - no scene reload)
     */
    bool Redo();

    /**
     * @brief Check if undo is available
     */
    bool CanUndo() const;

    /**
     * @brief Check if redo is available
     */
    bool CanRedo() const;

    /**
     * @brief Clear all undo/redo history
     */
    void Clear();

    /**
     * @brief Get undo stack size
     */
    size_t GetUndoCount() const;

    /**
     * @brief Get redo stack size
     */
    size_t GetRedoCount() const;

    /**
     * @brief Enable or disable the undo system
     */
    void SetSnapshotEnabled(bool enabled);

    /**
     * @brief Check if undo system is enabled
     */
    bool IsSnapshotEnabled() const;

private:
    SnapshotManager() = default;
    ~SnapshotManager() = default;
    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;
};
