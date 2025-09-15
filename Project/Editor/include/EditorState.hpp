#pragma once
#include <optional>
#include <cstdint>
#include "Engine.h"

// Forward declare Entity type
using Entity = uint32_t;
const Entity INVALID_ENTITY = 0xFFFFFFFF;

/**
 * @brief Manages the overall state of the editor (play/pause/stop) and selected entities.
 *
 * This singleton class controls whether the game is running or in edit mode,
 * similar to Unity's play/pause system, and tracks which entity is currently selected.
 */
class EditorState {
public:
    enum class State {
        EDIT_MODE,    // Editor mode - game logic paused, scene editing enabled
        PLAY_MODE,    // Play mode - game logic running
        PAUSED        // Play mode but paused
    };

    static EditorState& GetInstance();
    
    // State management (delegates to Engine)
    void SetState(State newState);
    State GetState() const;

    // Convenience methods (delegates to Engine)
    bool IsEditMode() const { return Engine::IsEditMode(); }
    bool IsPlayMode() const { return Engine::IsPlayMode(); }
    bool IsPaused() const { return Engine::IsPaused(); }
    bool ShouldRunGameLogic() const { return Engine::ShouldRunGameLogic(); }
    
    // State transitions
    void Play();
    void Pause();
    void Stop();

    // Entity selection management
    void SetSelectedEntity(Entity entity);
    void ClearSelection();
    Entity GetSelectedEntity() const { return m_SelectedEntity; }
    bool HasSelectedEntity() const { return m_SelectedEntity != INVALID_ENTITY; }
    
private:
    EditorState() = default;
    ~EditorState() = default;
    EditorState(const EditorState&) = delete;
    EditorState& operator=(const EditorState&) = delete;

    // Only store entity selection, game state is managed by Engine
    Entity m_SelectedEntity = INVALID_ENTITY;
};