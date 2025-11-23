#pragma once
#include "EditorPanel.hpp"
#include "Graphics/Sprite/SpriteAnimationComponent.hpp"
#include "ECS/Entity.hpp"
#include "imgui.h"
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

class Texture;
class FrameBuffer;

class SpriteAnimationEditorWindow : public EditorPanel {
public:
    SpriteAnimationEditorWindow() : EditorPanel("Sprite Animation Editor", false), m_EditorState{} {}
    ~SpriteAnimationEditorWindow() = default;

    void OnImGuiRender() override;

    // Open the editor for a specific entity's animation component
    void OpenForEntity(Entity entity, SpriteAnimationComponent* animComponent);
    void Close();
    bool IsEditingEntity(Entity entity) const { return entity == m_CurrentEntity && m_IsOpen; }

private:
    // Window state
    bool m_IsOpen = false;
    Entity m_CurrentEntity = 0;
    SpriteAnimationComponent* m_AnimComponent = nullptr;

    // Editor state
    struct EditorState {
        // Current clip being edited
        int selectedClipIndex = -1;
        int selectedFrameIndex = -1;

        // Timeline state
        float timelineZoom = 1.0f;
        float timelineScroll = 0.0f;
        float currentTime = 0.0f;
        bool isPlaying = false;
        float playbackSpeed = 1.0f;

        // Preview state
        float previewZoom = 1.0f;
        glm::vec2 previewPan = glm::vec2(0.0f);
        bool showGrid = true;
        bool showFrameBounds = true;
        int gridSize = 32;

        // Sprite sheet editor
        bool spriteSheetMode = false;
        GUID_128 currentTextureGUID{};  // Explicitly initialize
        std::shared_ptr<Texture> currentTexture;
        glm::vec2 selectionStart = glm::vec2(0.0f);
        glm::vec2 selectionEnd = glm::vec2(0.0f);
        bool isSelecting = false;

        // Onion skinning
        bool enableOnionSkin = false;
        int onionSkinBefore = 1;
        int onionSkinAfter = 1;
        float onionSkinAlpha = 0.3f;
    };
    EditorState m_EditorState;

    // Temporary edit buffer (for undo/redo)
    SpriteAnimationComponent m_EditBuffer;
    bool m_HasUnsavedChanges = false;

    // UI Layout constants
    static constexpr float TIMELINE_HEIGHT = 200.0f;
    static constexpr float PROPERTIES_WIDTH = 300.0f;
    static constexpr float TOOLBAR_HEIGHT = 40.0f;
    static constexpr float FRAME_WIDTH = 80.0f;
    static constexpr float TRACK_HEIGHT = 30.0f;

    // Rendering
    void DrawToolbar();
    void DrawClipSelector();
    void DrawTimeline();
    void DrawPreviewPanel();
    void DrawPropertiesPanel();
    void DrawSpriteSheetEditor();

    // Timeline helpers
    void DrawTimelineRuler(float width, float height);
    void DrawTimelineFrames(const SpriteAnimationClip& clip, float width, float height);
    void DrawTimelineCursor(float width, float height);
    void DrawFrameBlock(int frameIndex, float startTime, float duration, float y, float height, bool selected);

    // Preview helpers
    void DrawPreviewSprite();
    void DrawPreviewGrid();
    void DrawOnionSkin();
    void UpdatePreviewAnimation(float deltaTime);

    // Sprite sheet helpers
    void DrawSpriteSheetGrid();
    void DrawSpriteSheetSelection();
    void HandleSpriteSheetSelection();
    glm::vec4 GetUVFromSelection() const;

    // Frame operations
    void AddNewFrame(int clipIndex);
    void DeleteFrame(int clipIndex, int frameIndex);
    void DuplicateFrame(int clipIndex, int frameIndex);
    void MoveFrame(int clipIndex, int fromIndex, int toIndex);

    // Clip operations
    void AddNewClip();
    void DeleteClip(int clipIndex);
    void DuplicateClip(int clipIndex);
    void RenameClip(int clipIndex, const std::string& newName);

    // File operations
    void SaveAnimation();
    void UpdateSpriteRenderComponent();
    void LoadAnimation(const std::string& path);
    void ExportAnimation(const std::string& path);
    void ImportSpriteSheet(const std::string& path);

    // Utility
    float GetTotalClipDuration(const SpriteAnimationClip& clip) const;
    int GetFrameAtTime(const SpriteAnimationClip& clip, float time) const;
    float GetFrameStartTime(const SpriteAnimationClip& clip, int frameIndex) const;
    glm::vec2 ScreenToPreview(glm::vec2 screenPos) const;
    glm::vec2 PreviewToScreen(glm::vec2 previewPos) const;

    // Input handling
    void HandleKeyboardShortcuts();
    void HandleTimelineInput();
    void HandlePreviewInput();

    // Colors for UI
    static ImVec4 GetColorTimelineBg() { return ImVec4(0.15f, 0.15f, 0.15f, 1.0f); }
    static ImVec4 GetColorFrameNormal() { return ImVec4(0.3f, 0.5f, 0.7f, 1.0f); }
    static ImVec4 GetColorFrameSelected() { return ImVec4(0.5f, 0.7f, 1.0f, 1.0f); }
    static ImVec4 GetColorFrameHover() { return ImVec4(0.4f, 0.6f, 0.85f, 1.0f); }
    static ImVec4 GetColorTimelineCursor() { return ImVec4(1.0f, 0.3f, 0.3f, 1.0f); }
    static ImVec4 GetColorGrid() { return ImVec4(0.3f, 0.3f, 0.3f, 0.5f); }
    static ImVec4 GetColorSelection() { return ImVec4(0.2f, 0.7f, 1.0f, 0.3f); }
};

// Global instance accessor
SpriteAnimationEditorWindow* GetSpriteAnimationEditor();