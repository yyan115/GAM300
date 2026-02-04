/* Start Header ************************************************************************/
/*!
\file       UndoSystem.cpp
\author     Claude (Rewrite)
\date       2026
\brief      Implementation of Unity-style command-based undo/redo system.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "UndoSystem.hpp"
#include "Logging.hpp"

// ==================== GROUP COMMAND ====================

/**
 * @brief Command that groups multiple commands together
 */
class GroupCommand : public UndoCommand {
public:
    GroupCommand(std::vector<std::unique_ptr<UndoCommand>> commands, const std::string& description)
        : commands(std::move(commands))
        , description(description)
    {}

    void Execute() override {
        for (auto& cmd : commands) {
            cmd->Execute();
        }
    }

    void Undo() override {
        // Undo in reverse order
        for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
            (*it)->Undo();
        }
    }

    std::string GetDescription() const override { return description; }

private:
    std::vector<std::unique_ptr<UndoCommand>> commands;
    std::string description;
};

// ==================== UNDO SYSTEM ====================

UndoSystem& UndoSystem::GetInstance() {
    static UndoSystem instance;
    return instance;
}

void UndoSystem::PushCommand(std::unique_ptr<UndoCommand> command) {
    if (!enabled || !command) return;

    // If we're in a group, add to group instead
    if (groupDepth > 0) {
        currentGroup.push_back(std::move(command));
        return;
    }

    // Try to merge with the last command (for continuous edits like dragging)
    if (!undoStack.empty() && undoStack.back()->CanMerge()) {
        if (undoStack.back()->TryMerge(command.get())) {
            // Merged successfully, don't add new command
            // Clear redo stack since we modified history
            redoStack.clear();
            return;
        }
    }

    // Add to undo stack
    undoStack.push_back(std::move(command));

    // Maintain max size
    while (undoStack.size() > MAX_UNDO_COMMANDS) {
        undoStack.pop_front();
    }

    // Clear redo stack (new action invalidates redo history)
    redoStack.clear();

    ENGINE_LOG_INFO("[UndoSystem] Command pushed (Undo: " + std::to_string(undoStack.size()) + ")");
}

bool UndoSystem::Undo() {
    if (!CanUndo()) {
        ENGINE_LOG_WARN("[UndoSystem] Cannot undo - no history");
        return false;
    }

    // Pop from undo stack
    auto command = std::move(undoStack.back());
    undoStack.pop_back();

    // Undo the command (INSTANT - just restores a value)
    command->Undo();

    // Push to redo stack
    redoStack.push_back(std::move(command));

    // Maintain redo stack size
    while (redoStack.size() > MAX_UNDO_COMMANDS) {
        redoStack.pop_front();
    }

    ENGINE_LOG_INFO("[UndoSystem] Undo performed (Undo: " + std::to_string(undoStack.size()) +
                    ", Redo: " + std::to_string(redoStack.size()) + ")");
    return true;
}

bool UndoSystem::Redo() {
    if (!CanRedo()) {
        ENGINE_LOG_WARN("[UndoSystem] Cannot redo - no history");
        return false;
    }

    // Pop from redo stack
    auto command = std::move(redoStack.back());
    redoStack.pop_back();

    // Execute the command (INSTANT - just applies a value)
    command->Execute();

    // Push back to undo stack
    undoStack.push_back(std::move(command));

    // Maintain undo stack size
    while (undoStack.size() > MAX_UNDO_COMMANDS) {
        undoStack.pop_front();
    }

    ENGINE_LOG_INFO("[UndoSystem] Redo performed (Undo: " + std::to_string(undoStack.size()) +
                    ", Redo: " + std::to_string(redoStack.size()) + ")");
    return true;
}

void UndoSystem::Clear() {
    undoStack.clear();
    redoStack.clear();
    currentGroup.clear();
    groupDepth = 0;
    ENGINE_LOG_INFO("[UndoSystem] History cleared");
}

void UndoSystem::BeginGroup(const std::string& description) {
    groupDepth++;
    if (groupDepth == 1) {
        currentGroupDescription = description;
        currentGroup.clear();
    }
}

void UndoSystem::EndGroup() {
    if (groupDepth <= 0) return;

    groupDepth--;
    if (groupDepth == 0 && !currentGroup.empty()) {
        // Create a group command from all collected commands
        PushCommand(std::make_unique<GroupCommand>(std::move(currentGroup), currentGroupDescription));
        currentGroup.clear();
    }
}

// ==================== LEGACY COMPATIBILITY ====================

namespace LegacyUndo {
    static bool inHeavyOperation = false;
    static std::string heavyOperationDescription;

    void TakeSnapshot(const std::string& description) {
        // For most property edits, the new system handles this automatically.
        // This function is now a no-op for simple edits.
        //
        // The new UndoableWidgets will directly use UndoSystem::RecordPropertyChange()
        // which is instant and doesn't serialize anything.
        (void)description;

        // If we're in a heavy operation, mark that something changed
        // (the heavy operation handler will deal with it)
    }

    void BeginHeavyOperation(const std::string& description) {
        inHeavyOperation = true;
        heavyOperationDescription = description;
        UndoSystem::GetInstance().BeginGroup(description);
    }

    void EndHeavyOperation() {
        inHeavyOperation = false;
        UndoSystem::GetInstance().EndGroup();
    }
}
