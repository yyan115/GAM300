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

    enum class ViewMode {
        VIEW_3D,      // 3D mode - show 3D models and 3D sprites
        VIEW_2D       // 2D mode - show 2D sprites only in screen space
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

    // View mode management
    void SetViewMode(ViewMode mode) { viewMode = mode; }
    ViewMode GetViewMode() const { return viewMode; }
    bool Is3DMode() const { return viewMode == ViewMode::VIEW_3D; }
    bool Is2DMode() const { return viewMode == ViewMode::VIEW_2D; }

    // Entity selection management
    void SetSelectedEntity(Entity entity);
    void ClearSelection();
    Entity GetSelectedEntity() const { return selectedEntity; }
    bool HasSelectedEntity() const { return selectedEntity != INVALID_ENTITY; }
    
private:
    EditorState() = default;
    ~EditorState() = default;
    EditorState(const EditorState&) = delete;
    EditorState& operator=(const EditorState&) = delete;

    // Only store entity selection, game state is managed by Engine
    Entity selectedEntity = INVALID_ENTITY;

    // View mode state
    ViewMode viewMode = ViewMode::VIEW_3D;
};