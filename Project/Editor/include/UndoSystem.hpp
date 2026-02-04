/* Start Header ************************************************************************/
/*!
\file       UndoSystem.hpp
\author     Claude (Rewrite)
\date       2026
\brief      Unity-style command-based undo/redo system.

            Instead of serializing the entire scene on every edit (slow), this system
            stores lightweight commands that record only what changed:
            - Property changes: {entity, component, field, oldValue, newValue}
            - Entity operations: {entityData} for create/delete

            Undo = restore old value (instant)
            Redo = apply new value (instant)

            No scene reload, no resource reloading, no animation rebuilding.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <deque>
#include <variant>
#include <cstring>
#include <chrono>
#include "ECS/Entity.hpp"

/**
 * @brief Base class for all undoable commands (Command Pattern)
 */
class UndoCommand {
public:
    virtual ~UndoCommand() = default;

    /**
     * @brief Execute the command (apply the change)
     */
    virtual void Execute() = 0;

    /**
     * @brief Undo the command (restore previous state)
     */
    virtual void Undo() = 0;

    /**
     * @brief Get description for debugging/UI
     */
    virtual std::string GetDescription() const = 0;

    /**
     * @brief Try to merge with another command (for continuous edits like dragging)
     * @return true if merged successfully, false if commands should remain separate
     */
    virtual bool TryMerge(const UndoCommand* other) { (void)other; return false; }

    /**
     * @brief Check if this command can be merged with subsequent commands
     */
    virtual bool CanMerge() const { return false; }

    /**
     * @brief Get the timestamp when this command was created
     */
    uint64_t GetTimestamp() const { return timestamp; }

protected:
    uint64_t timestamp = 0;
};

/**
 * @brief Command for simple property changes (most common case)
 *
 * Stores the memory address, old value, and new value.
 * Undo/redo simply copies the appropriate value back.
 */
template<typename T>
class PropertyCommand : public UndoCommand {
public:
    PropertyCommand(T* target, const T& oldValue, const T& newValue, const std::string& description)
        : target(target)
        , oldValue(oldValue)
        , newValue(newValue)
        , description(description)
    {
        timestamp = GetCurrentTimestamp();
    }

    void Execute() override {
        if (target) *target = newValue;
    }

    void Undo() override {
        if (target) *target = oldValue;
    }

    std::string GetDescription() const override { return description; }

    bool CanMerge() const override { return true; }

    bool TryMerge(const UndoCommand* other) override {
        auto* otherProp = dynamic_cast<const PropertyCommand<T>*>(other);
        if (!otherProp) return false;

        // Only merge if same target and within 500ms
        if (otherProp->target != target) return false;
        if (otherProp->timestamp - timestamp > 500) return false;

        // Merge: keep our oldValue, take their newValue
        newValue = otherProp->newValue;
        timestamp = otherProp->timestamp;
        return true;
    }

private:
    T* target;
    T oldValue;
    T newValue;
    std::string description;

    static uint64_t GetCurrentTimestamp() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }
};

/**
 * @brief Command for array/vector property changes (float[3], float[4], etc.)
 */
template<typename T, size_t N>
class ArrayPropertyCommand : public UndoCommand {
public:
    ArrayPropertyCommand(T* target, const T (&oldValue)[N], const T (&newValue)[N], const std::string& description)
        : target(target)
        , description(description)
    {
        std::memcpy(this->oldValue, oldValue, sizeof(T) * N);
        std::memcpy(this->newValue, newValue, sizeof(T) * N);
        timestamp = GetCurrentTimestamp();
    }

    void Execute() override {
        if (target) std::memcpy(target, newValue, sizeof(T) * N);
    }

    void Undo() override {
        if (target) std::memcpy(target, oldValue, sizeof(T) * N);
    }

    std::string GetDescription() const override { return description; }

    bool CanMerge() const override { return true; }

    bool TryMerge(const UndoCommand* other) override {
        auto* otherArr = dynamic_cast<const ArrayPropertyCommand<T, N>*>(other);
        if (!otherArr) return false;

        // Only merge if same target and within 500ms
        if (otherArr->target != target) return false;
        if (otherArr->timestamp - timestamp > 500) return false;

        // Merge: keep our oldValue, take their newValue
        std::memcpy(newValue, otherArr->newValue, sizeof(T) * N);
        timestamp = otherArr->timestamp;
        return true;
    }

private:
    T* target;
    T oldValue[N];
    T newValue[N];
    std::string description;

    static uint64_t GetCurrentTimestamp() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }
};

/**
 * @brief Command for string property changes
 */
class StringPropertyCommand : public UndoCommand {
public:
    StringPropertyCommand(std::string* target, const std::string& oldValue, const std::string& newValue, const std::string& description)
        : target(target)
        , oldValue(oldValue)
        , newValue(newValue)
        , description(description)
    {
        timestamp = GetCurrentTimestamp();
    }

    void Execute() override {
        if (target) *target = newValue;
    }

    void Undo() override {
        if (target) *target = oldValue;
    }

    std::string GetDescription() const override { return description; }

    bool CanMerge() const override { return true; }

    bool TryMerge(const UndoCommand* other) override {
        auto* otherStr = dynamic_cast<const StringPropertyCommand*>(other);
        if (!otherStr) return false;

        if (otherStr->target != target) return false;
        if (otherStr->timestamp - timestamp > 500) return false;

        newValue = otherStr->newValue;
        timestamp = otherStr->timestamp;
        return true;
    }

private:
    std::string* target;
    std::string oldValue;
    std::string newValue;
    std::string description;

    static uint64_t GetCurrentTimestamp() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }
};

/**
 * @brief Command for char buffer changes (InputText)
 */
class CharBufferCommand : public UndoCommand {
public:
    CharBufferCommand(char* target, size_t bufSize, const std::string& oldValue, const std::string& newValue, const std::string& description)
        : target(target)
        , bufSize(bufSize)
        , oldValue(oldValue)
        , newValue(newValue)
        , description(description)
    {
        timestamp = GetCurrentTimestamp();
    }

    void Execute() override {
        if (target) {
            #ifdef _MSC_VER
                strncpy_s(target, bufSize, newValue.c_str(), _TRUNCATE);
            #else
                std::strncpy(target, newValue.c_str(), bufSize - 1);
                target[bufSize - 1] = '\0';
            #endif
        }
    }

    void Undo() override {
        if (target) {
            #ifdef _MSC_VER
                strncpy_s(target, bufSize, oldValue.c_str(), _TRUNCATE);
            #else
                std::strncpy(target, oldValue.c_str(), bufSize - 1);
                target[bufSize - 1] = '\0';
            #endif
        }
    }

    std::string GetDescription() const override { return description; }

    bool CanMerge() const override { return true; }

    bool TryMerge(const UndoCommand* other) override {
        auto* otherBuf = dynamic_cast<const CharBufferCommand*>(other);
        if (!otherBuf) return false;

        if (otherBuf->target != target) return false;
        if (otherBuf->timestamp - timestamp > 500) return false;

        newValue = otherBuf->newValue;
        timestamp = otherBuf->timestamp;
        return true;
    }

private:
    char* target;
    size_t bufSize;
    std::string oldValue;
    std::string newValue;
    std::string description;

    static uint64_t GetCurrentTimestamp() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }
};

/**
 * @brief Command for generic lambda-based undo/redo
 *
 * Use this for complex operations that can't be expressed as simple property changes.
 */
class LambdaCommand : public UndoCommand {
public:
    LambdaCommand(std::function<void()> doFunc, std::function<void()> undoFunc, const std::string& description)
        : doFunc(std::move(doFunc))
        , undoFunc(std::move(undoFunc))
        , description(description)
    {}

    void Execute() override { if (doFunc) doFunc(); }
    void Undo() override { if (undoFunc) undoFunc(); }
    std::string GetDescription() const override { return description; }

private:
    std::function<void()> doFunc;
    std::function<void()> undoFunc;
    std::string description;
};

/**
 * @brief Unity-style command-based undo/redo manager
 *
 * Key differences from the old SnapshotManager:
 * - Stores lightweight commands instead of full scene JSON
 * - Undo/redo is instant (just restore a value)
 * - No file I/O, no scene reload, no resource reloading
 * - Merges continuous edits (dragging) into single commands
 */
class UndoSystem {
public:
    static UndoSystem& GetInstance();

    /**
     * @brief Push a command onto the undo stack
     *
     * The command is automatically executed. If merging is enabled and the
     * command can merge with the previous command, they are combined.
     */
    void PushCommand(std::unique_ptr<UndoCommand> command);

    /**
     * @brief Undo the last command (instant)
     * @return true if undo was performed
     */
    bool Undo();

    /**
     * @brief Redo the last undone command (instant)
     * @return true if redo was performed
     */
    bool Redo();

    /**
     * @brief Check if undo is available
     */
    bool CanUndo() const { return !undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    bool CanRedo() const { return !redoStack.empty(); }

    /**
     * @brief Clear all undo/redo history
     */
    void Clear();

    /**
     * @brief Get undo stack size
     */
    size_t GetUndoCount() const { return undoStack.size(); }

    /**
     * @brief Get redo stack size
     */
    size_t GetRedoCount() const { return redoStack.size(); }

    /**
     * @brief Enable or disable the undo system
     */
    void SetEnabled(bool enabled) { this->enabled = enabled; }

    /**
     * @brief Check if undo system is enabled
     */
    bool IsEnabled() const { return enabled; }

    /**
     * @brief Begin a group of commands that should be undone together
     */
    void BeginGroup(const std::string& description);

    /**
     * @brief End the current command group
     */
    void EndGroup();

    /**
     * @brief Check if currently recording a command group
     */
    bool IsInGroup() const { return groupDepth > 0; }

    // ==================== CONVENIENCE METHODS ====================

    /**
     * @brief Record a property change (most common use case)
     */
    template<typename T>
    void RecordPropertyChange(T* target, const T& oldValue, const T& newValue, const std::string& description) {
        if (!enabled) return;
        PushCommand(std::make_unique<PropertyCommand<T>>(target, oldValue, newValue, description));
    }

    /**
     * @brief Record an array property change
     */
    template<typename T, size_t N>
    void RecordArrayChange(T* target, const T (&oldValue)[N], const T (&newValue)[N], const std::string& description) {
        if (!enabled) return;
        PushCommand(std::make_unique<ArrayPropertyCommand<T, N>>(target, oldValue, newValue, description));
    }

    /**
     * @brief Record a string change
     */
    void RecordStringChange(std::string* target, const std::string& oldValue, const std::string& newValue, const std::string& description) {
        if (!enabled) return;
        PushCommand(std::make_unique<StringPropertyCommand>(target, oldValue, newValue, description));
    }

    /**
     * @brief Record a char buffer change
     */
    void RecordCharBufferChange(char* target, size_t bufSize, const std::string& oldValue, const std::string& newValue, const std::string& description) {
        if (!enabled) return;
        PushCommand(std::make_unique<CharBufferCommand>(target, bufSize, oldValue, newValue, description));
    }

    /**
     * @brief Record a generic lambda-based change
     */
    void RecordLambdaChange(std::function<void()> doFunc, std::function<void()> undoFunc, const std::string& description) {
        if (!enabled) return;
        PushCommand(std::make_unique<LambdaCommand>(std::move(doFunc), std::move(undoFunc), description));
    }

private:
    UndoSystem() = default;
    ~UndoSystem() = default;
    UndoSystem(const UndoSystem&) = delete;
    UndoSystem& operator=(const UndoSystem&) = delete;

    static constexpr size_t MAX_UNDO_COMMANDS = 100;

    std::deque<std::unique_ptr<UndoCommand>> undoStack;
    std::deque<std::unique_ptr<UndoCommand>> redoStack;
    bool enabled = true;

    // Group support (for compound operations)
    int groupDepth = 0;
    std::vector<std::unique_ptr<UndoCommand>> currentGroup;
    std::string currentGroupDescription;
};

// ==================== LEGACY COMPATIBILITY ====================

/**
 * @brief Compatibility wrapper - redirects old SnapshotManager calls to UndoSystem
 *
 * This allows gradual migration. Old code calling SnapshotManager::TakeSnapshot()
 * will now do nothing (or optionally create a checkpoint for heavy operations).
 */
namespace LegacyUndo {
    /**
     * @brief Called by old code expecting to take a snapshot
     *
     * For simple property edits, this does nothing (the new system handles it).
     * For heavy operations (delete entity, etc.), this creates a proper command.
     */
    void TakeSnapshot(const std::string& description);

    /**
     * @brief Mark the start of a heavy operation that needs full entity backup
     */
    void BeginHeavyOperation(const std::string& description);

    /**
     * @brief Mark the end of a heavy operation
     */
    void EndHeavyOperation();
}
