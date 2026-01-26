#pragma once
#include "EditorPanel.hpp"
#include "Animation/AnimatorController.hpp"
#include "Animation/AnimationStateMachine.hpp"
#include "Animation/AnimationComponent.hpp"
#include "ECS/Entity.hpp"
#include "imgui.h"
#include <memory>
#include <string>
#include <optional>

// Forward declarations
class AnimatorController;
class AnimationComponent;

class AnimatorEditorWindow : public EditorPanel {
public:
    AnimatorEditorWindow();
    ~AnimatorEditorWindow() = default;

    void OnImGuiRender() override;

    // Open the editor for a specific entity's animation component
    void OpenForEntity(Entity entity, AnimationComponent* animComponent);

    // Open the editor for a standalone controller file
    void OpenController(const std::string& filePath);

    // Create a new controller
    void CreateNewController();

    void Close();
    bool IsEditingEntity(Entity entity) const { return entity == m_CurrentEntity && IsOpen(); }

    // Get the current controller being edited
    AnimatorController* GetController() { return m_Controller.get(); }

private:
    // Window state
    Entity m_CurrentEntity = 0;
    AnimationComponent* m_AnimComponent = nullptr;
    std::unique_ptr<AnimatorController> m_Controller;
    std::string m_ControllerFilePath;
    bool m_HasUnsavedChanges = false;

    // Node graph view state
    ImVec2 m_ViewOffset = {0.0f, 0.0f};     // Pan offset
    float m_ViewZoom = 1.0f;                 // Zoom level
    ImVec2 m_CanvasSize = {0.0f, 0.0f};
    ImVec2 m_CanvasPos = {0.0f, 0.0f};

    // Selection state
    enum class SelectionType { None, State, Transition, EntryNode, AnyStateNode };
    SelectionType m_SelectionType = SelectionType::None;
    std::string m_SelectedStateId;
    size_t m_SelectedTransitionIndex = 0;

    // Interaction state
    bool m_IsDraggingNode = false;
    bool m_IsDraggingCanvas = false;
    bool m_IsCreatingTransition = false;
    std::string m_TransitionFromState;
    ImVec2 m_TransitionEndPos;

    // Context menu state
    bool m_ShowContextMenu = false;
    ImVec2 m_ContextMenuPos;
    std::string m_ContextMenuStateId;

    // Renaming state
    bool m_IsRenaming = false;
    char m_RenameBuffer[256] = {0};

    // Layout constants
    static constexpr float NODE_WIDTH = 150.0f;
    static constexpr float NODE_HEIGHT = 40.0f;
    static constexpr float NODE_ROUNDING = 4.0f;
    static constexpr float TOOLBAR_HEIGHT = 30.0f;
    static constexpr float SPLITTER_THICKNESS = 4.0f;
    static constexpr float MIN_PANEL_WIDTH = 150.0f;

    // Resizable panel widths
    float m_ParameterPanelWidth = 200.0f;
    float m_InspectorPanelWidth = 250.0f;

    // Main rendering methods
    void DrawToolbar();
    void DrawParameterPanel();
    void DrawNodeGraph();
    void DrawInspectorPanel();

    // Node graph rendering
    void DrawGrid();
    void DrawStates();
    void DrawTransitions();
    void DrawEntryNode();
    void DrawAnyStateNode();
    void DrawStateNode(const std::string& stateId, AnimStateConfig& config);
    void DrawTransitionArrow(const ImVec2& from, const ImVec2& to, bool isSelected, bool isFromAnyState = false, float perpOffset = 0.0f);
    void DrawTransitionCreationLine();

    // Coordinate helpers
    ImVec2 WorldToScreen(const ImVec2& worldPos) const;
    ImVec2 ScreenToWorld(const ImVec2& screenPos) const;
    ImVec2 GetStateNodeCenter(const std::string& stateId) const;
    ImVec2 GetEntryNodeCenter() const;
    ImVec2 GetAnyStateNodeCenter() const;

    // Input handling
    void HandleCanvasInput(bool clickedOnItem = false);
    void HandleNodeDragging();
    void HandleTransitionCreation();
    void HandleContextMenu();
    void HandleKeyboardShortcuts();

    // State operations
    void CreateNewState(const ImVec2& position);
    void DeleteSelectedState();
    void DuplicateSelectedState();
    void SetAsEntryState(const std::string& stateId);
    void RenameState(const std::string& oldName, const std::string& newName);

    // Transition operations
    void CreateTransition(const std::string& fromState, const std::string& toState);
    void DeleteSelectedTransition();

    // Parameter operations
    void DrawParameterList();
    void AddParameter(AnimParamType type);
    void DeleteParameter(const std::string& name);

    // Inspector helpers
    void DrawStateInspector();
    void DrawTransitionInspector();
    void DrawConditionEditor(AnimTransition& transition);

    // File operations
    void SaveController();
    void SaveControllerAs();
    void LoadController();
    void ApplyToAnimationComponent();

    // Utility
    std::string GenerateUniqueStateName(const std::string& baseName = "New State");
    std::string GenerateUniqueParamName(const std::string& baseName = "New Parameter");
    bool IsPointInNode(const ImVec2& point, const ImVec2& nodePos, const ImVec2& nodeSize) const;
    std::optional<std::string> GetStateAtPosition(const ImVec2& screenPos) const;
    std::string OpenAnimationFileDialog();
    std::string GetClipDisplayName(const std::string& path) const;

    // Colors
    static ImU32 GetStateColor(bool isSelected, bool isEntry);
    static ImU32 GetTransitionColor(bool isSelected);
    static ImU32 GetEntryNodeColor();
    static ImU32 GetAnyStateNodeColor();
};

// Global instance accessor
AnimatorEditorWindow* GetAnimatorEditor();
