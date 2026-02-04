/* Start Header ************************************************************************/
/*!
\file       SnapshotManager.cpp
\author     Claude (Rewrite)
\date       2026
\brief      Compatibility wrapper implementation - redirects to UndoSystem.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "SnapshotManager.hpp"
#include "UndoSystem.hpp"
#include "Logging.hpp"

SnapshotManager& SnapshotManager::GetInstance() {
    static SnapshotManager instance;
    return instance;
}

void SnapshotManager::TakeSnapshot(const std::string& description) {
    // This is now a no-op for simple property edits.
    // The new UndoableWidgets handle property changes automatically.
    //
    // For heavy operations (delete entity, etc.), use UndoSystem directly
    // with a LambdaCommand that can properly undo the operation.
    (void)description;

    // Log for debugging during transition
    // ENGINE_LOG_INFO("[SnapshotManager] TakeSnapshot called (legacy, no-op): " + description);
}

bool SnapshotManager::Undo() {
    // Redirect to new UndoSystem - INSTANT, no scene reload
    return UndoSystem::GetInstance().Undo();
}

bool SnapshotManager::Redo() {
    // Redirect to new UndoSystem - INSTANT, no scene reload
    return UndoSystem::GetInstance().Redo();
}

bool SnapshotManager::CanUndo() const {
    return UndoSystem::GetInstance().CanUndo();
}

bool SnapshotManager::CanRedo() const {
    return UndoSystem::GetInstance().CanRedo();
}

void SnapshotManager::Clear() {
    UndoSystem::GetInstance().Clear();
}

size_t SnapshotManager::GetUndoCount() const {
    return UndoSystem::GetInstance().GetUndoCount();
}

size_t SnapshotManager::GetRedoCount() const {
    return UndoSystem::GetInstance().GetRedoCount();
}

void SnapshotManager::SetSnapshotEnabled(bool enabled) {
    UndoSystem::GetInstance().SetEnabled(enabled);
}

bool SnapshotManager::IsSnapshotEnabled() const {
    return UndoSystem::GetInstance().IsEnabled();
}
