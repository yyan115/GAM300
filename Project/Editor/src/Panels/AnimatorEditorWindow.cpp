#include "pch.h"
#include "Panels/AnimatorEditorWindow.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include "GUIManager.hpp"
#include "SnapshotManager.hpp"
#include "Logging.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <map>

#ifdef _WIN32
#define NOMINMAX  // Prevent Windows.h from defining min/max macros
#include <Windows.h>
#include <shobjidl.h>
#endif

// Global instance
static AnimatorEditorWindow* g_AnimatorEditor = nullptr;

AnimatorEditorWindow* GetAnimatorEditor() {
    if (!g_AnimatorEditor) {
        g_AnimatorEditor = new AnimatorEditorWindow();
    }
    return g_AnimatorEditor;
}

AnimatorEditorWindow::AnimatorEditorWindow()
    : EditorPanel("Animator", false)
{
    m_Controller = std::make_unique<AnimatorController>();
}

void AnimatorEditorWindow::OnImGuiRender()
{
    if (!IsOpen()) return;

    ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);

    std::string windowTitle = "Animator";
    if (!m_ControllerFilePath.empty()) {
        size_t lastSlash = m_ControllerFilePath.find_last_of("/\\");
        std::string fileName = (lastSlash != std::string::npos)
            ? m_ControllerFilePath.substr(lastSlash + 1)
            : m_ControllerFilePath;
        windowTitle += " - " + fileName;
    }
    if (m_HasUnsavedChanges) {
        windowTitle += "*";
    }
    windowTitle += "###AnimatorWindow";

    bool windowOpen = IsOpen();
    if (ImGui::Begin(windowTitle.c_str(), &windowOpen, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem(ICON_FA_FILE " New")) {
                    CreateNewController();
                }
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open...")) {
                    LoadController();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save", "Ctrl+S")) {
                    SaveController();
                }
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save As...")) {
                    SaveControllerAs();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem(ICON_FA_PLUS " Add State", "")) {
                    CreateNewState(ScreenToWorld(ImVec2(m_CanvasPos.x + m_CanvasSize.x * 0.5f,
                                                        m_CanvasPos.y + m_CanvasSize.y * 0.5f)));
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_TRASH " Delete Selected", "Delete", false,
                    m_SelectionType == SelectionType::State || m_SelectionType == SelectionType::Transition)) {
                    if (m_SelectionType == SelectionType::State) DeleteSelectedState();
                    else if (m_SelectionType == SelectionType::Transition) DeleteSelectedTransition();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        DrawToolbar();

        // Main content area with three panels and splitters
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float availableHeight = ImGui::GetContentRegionAvail().y;

        // Clamp panel widths to valid range
        float minGraphWidth = 200.0f;
        float maxParamWidth = availableWidth - m_InspectorPanelWidth - minGraphWidth - SPLITTER_THICKNESS * 2;
        float maxInspectorWidth = availableWidth - m_ParameterPanelWidth - minGraphWidth - SPLITTER_THICKNESS * 2;
        m_ParameterPanelWidth = std::clamp(m_ParameterPanelWidth, MIN_PANEL_WIDTH, maxParamWidth);
        m_InspectorPanelWidth = std::clamp(m_InspectorPanelWidth, MIN_PANEL_WIDTH, maxInspectorWidth);

        float graphWidth = availableWidth - m_ParameterPanelWidth - m_InspectorPanelWidth - SPLITTER_THICKNESS * 2;

        // Parameter panel (left)
        ImGui::BeginChild("ParameterPanel", ImVec2(m_ParameterPanelWidth, 0), true);
        DrawParameterPanel();
        ImGui::EndChild();

        // Left splitter
        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Button("##LeftSplitter", ImVec2(SPLITTER_THICKNESS, availableHeight));
        if (ImGui::IsItemActive()) {
            m_ParameterPanelWidth += ImGui::GetIO().MouseDelta.x;
            m_ParameterPanelWidth = std::clamp(m_ParameterPanelWidth, MIN_PANEL_WIDTH, maxParamWidth);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        ImGui::PopStyleColor(3);

        // Node graph (center)
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("NodeGraphPanel", ImVec2(graphWidth, 0), true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawNodeGraph();
        ImGui::EndChild();

        // Right splitter
        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Button("##RightSplitter", ImVec2(SPLITTER_THICKNESS, availableHeight));
        if (ImGui::IsItemActive()) {
            m_InspectorPanelWidth -= ImGui::GetIO().MouseDelta.x;
            m_InspectorPanelWidth = std::clamp(m_InspectorPanelWidth, MIN_PANEL_WIDTH, maxInspectorWidth);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        ImGui::PopStyleColor(3);

        // Inspector panel (right)
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("InspectorPanel", ImVec2(m_InspectorPanelWidth, 0), true);
        DrawInspectorPanel();
        ImGui::EndChild();
    }
    ImGui::End();

    // Handle close button click (after ImGui::End so the window is properly closed)
    if (!windowOpen) {
        SetOpen(false);
    }

    HandleKeyboardShortcuts();
}

void AnimatorEditorWindow::OpenForEntity(Entity entity, AnimationComponent* animComponent)
{
    m_CurrentEntity = entity;
    m_AnimComponent = animComponent;
    SetOpen(true);  // Use base class method
    m_ControllerFilePath.clear();
    m_HasUnsavedChanges = false;

    // Extract state machine data if exists
    if (animComponent) {
        AnimationStateMachine* sm = animComponent->GetStateMachine();
        if (sm) {
            m_Controller->ExtractFromStateMachine(sm);
        } else {
            m_Controller = std::make_unique<AnimatorController>();
        }

        // Copy clip paths from animation component
        m_Controller->GetClipPaths() = animComponent->clipPaths;
    }

    // Reset view
    m_ViewOffset = ImVec2(0, 0);
    m_ViewZoom = 1.0f;
    m_SelectionType = SelectionType::None;
}

void AnimatorEditorWindow::OpenController(const std::string& filePath)
{
    m_Controller = std::make_unique<AnimatorController>();
    if (m_Controller->LoadFromFile(filePath)) {
        m_ControllerFilePath = filePath;
        SetOpen(true);  // Use base class method
        m_HasUnsavedChanges = false;
        m_CurrentEntity = 0;
        m_AnimComponent = nullptr;
    }

    // Reset view
    m_ViewOffset = ImVec2(0, 0);
    m_ViewZoom = 1.0f;
    m_SelectionType = SelectionType::None;
}

void AnimatorEditorWindow::CreateNewController()
{
    m_Controller = std::make_unique<AnimatorController>();
    m_ControllerFilePath.clear();
    SetOpen(true);  // Use base class method
    m_HasUnsavedChanges = false;
    m_CurrentEntity = 0;
    m_AnimComponent = nullptr;

    // Reset view
    m_ViewOffset = ImVec2(0, 0);
    m_ViewZoom = 1.0f;
    m_SelectionType = SelectionType::None;
}

void AnimatorEditorWindow::Close()
{
    SetOpen(false);  // Use base class method
    m_CurrentEntity = 0;
    m_AnimComponent = nullptr;
}

void AnimatorEditorWindow::DrawToolbar()
{
    ImGui::BeginChild("Toolbar", ImVec2(0, TOOLBAR_HEIGHT), false);

    // Zoom controls
    ImGui::Text("Zoom: %.0f%%", m_ViewZoom * 100.0f);
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS_MINUS)) {
        m_ViewZoom = std::max(0.25f, m_ViewZoom - 0.25f);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS_PLUS)) {
        m_ViewZoom = std::min(2.0f, m_ViewZoom + 0.25f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        m_ViewOffset = ImVec2(0, 0);
        m_ViewZoom = 1.0f;
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Apply button (when editing entity)
    if (m_AnimComponent) {
        if (ImGui::Button(ICON_FA_CHECK " Apply to Entity")) {
            ApplyToAnimationComponent();
        }
        ImGui::SameLine();
    }

    // Auto-layout button
    if (ImGui::Button(ICON_FA_WAND_MAGIC_SPARKLES " Auto Layout")) {
        // Simple auto-layout: arrange states in a grid
        int i = 0;
        int cols = 4;
        for (auto& [stateId, config] : m_Controller->GetStates()) {
            int row = i / cols;
            int col = i % cols;
            config.nodePosition = glm::vec2(col * 200.0f, row * 100.0f);
            i++;
        }
        m_HasUnsavedChanges = true;
    }

    ImGui::EndChild();
    ImGui::Separator();
}

void AnimatorEditorWindow::DrawParameterPanel()
{
    ImGui::Text(ICON_FA_SLIDERS " Parameters");
    ImGui::Separator();

    // Add parameter buttons
    if (ImGui::Button(ICON_FA_PLUS " Bool")) AddParameter(AnimParamType::Bool);
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS " Int")) AddParameter(AnimParamType::Int);
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS " Float")) AddParameter(AnimParamType::Float);
    if (ImGui::Button(ICON_FA_PLUS " Trigger")) AddParameter(AnimParamType::Trigger);

    ImGui::Separator();

    DrawParameterList();
}

void AnimatorEditorWindow::DrawParameterList()
{
    auto& params = m_Controller->GetParameters();

    for (size_t i = 0; i < params.size(); i++) {
        auto& param = params[i];
        ImGui::PushID(static_cast<int>(i));

        // Type icon
        const char* typeIcon = "";
        switch (param.type) {
            case AnimParamType::Bool: typeIcon = ICON_FA_TOGGLE_ON; break;
            case AnimParamType::Int: typeIcon = ICON_FA_HASHTAG; break;
            case AnimParamType::Float: typeIcon = ICON_FA_PERCENT; break;
            case AnimParamType::Trigger: typeIcon = ICON_FA_BOLT; break;
        }
        ImGui::Text("%s", typeIcon);
        ImGui::SameLine();

        // Editable name
        char nameBuf[128];
        strncpy(nameBuf, param.name.c_str(), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (strlen(nameBuf) > 0 && strcmp(nameBuf, param.name.c_str()) != 0) {
                m_Controller->RenameParameter(param.name, nameBuf);
                m_HasUnsavedChanges = true;
            }
        }

        ImGui::SameLine();

        // Delete button
        if (ImGui::Button(ICON_FA_TRASH "##Delete")) {
            DeleteParameter(param.name);
            ImGui::PopID();
            break;
        }

        // Default value editor
        ImGui::SetNextItemWidth(60);
        switch (param.type) {
            case AnimParamType::Bool: {
                bool boolVal = param.defaultValue > 0.5f;
                if (ImGui::Checkbox("##BoolVal", &boolVal)) {
                    param.defaultValue = boolVal ? 1.0f : 0.0f;
                    m_HasUnsavedChanges = true;
                }
                break;
            }
            case AnimParamType::Int: {
                int intVal = static_cast<int>(param.defaultValue);
                if (ImGui::InputInt("##IntVal", &intVal)) {
                    param.defaultValue = static_cast<float>(intVal);
                    m_HasUnsavedChanges = true;
                }
                break;
            }
            case AnimParamType::Float: {
                if (ImGui::InputFloat("##FloatVal", &param.defaultValue, 0.1f, 1.0f, "%.2f")) {
                    m_HasUnsavedChanges = true;
                }
                break;
            }
            case AnimParamType::Trigger:
                ImGui::TextDisabled("(trigger)");
                break;
        }

        ImGui::PopID();
    }
}

void AnimatorEditorWindow::DrawNodeGraph()
{
    m_CanvasPos = ImGui::GetCursorScreenPos();
    m_CanvasSize = ImGui::GetContentRegionAvail();

    // Check if mouse is in canvas bounds
    ImVec2 mousePos = ImGui::GetMousePos();
    bool mouseInCanvas = mousePos.x >= m_CanvasPos.x && mousePos.x <= m_CanvasPos.x + m_CanvasSize.x &&
                         mousePos.y >= m_CanvasPos.y && mousePos.y <= m_CanvasPos.y + m_CanvasSize.y;

    // Capture click state BEFORE InvisibleButton consumes it
    bool leftClicked = mouseInCanvas && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool rightClicked = mouseInCanvas && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    bool leftDoubleClicked = mouseInCanvas && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    // Add invisible button to capture mouse input and prevent window dragging
    ImGui::InvisibleButton("NodeGraphCanvas", m_CanvasSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool isCanvasHovered = ImGui::IsItemHovered();
    bool isCanvasActive = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(m_CanvasPos,
        ImVec2(m_CanvasPos.x + m_CanvasSize.x, m_CanvasPos.y + m_CanvasSize.y),
        IM_COL32(30, 30, 30, 255));

    // Clip to canvas
    drawList->PushClipRect(m_CanvasPos,
        ImVec2(m_CanvasPos.x + m_CanvasSize.x, m_CanvasPos.y + m_CanvasSize.y), true);

    DrawGrid();
    DrawEntryNode();
    DrawAnyStateNode();

    // Handle click detection for states
    bool clickedOnState = false;
    for (auto& [stateId, config] : m_Controller->GetStates()) {
        ImVec2 worldPos(config.nodePosition.x, config.nodePosition.y);
        ImVec2 screenPos = WorldToScreen(worldPos);
        ImVec2 nodeSize(NODE_WIDTH * m_ViewZoom, NODE_HEIGHT * m_ViewZoom);

        if (IsPointInNode(mousePos, screenPos, nodeSize)) {
            if (leftClicked) {
                if (m_IsCreatingTransition) {
                    CreateTransition(m_TransitionFromState, stateId);
                    m_IsCreatingTransition = false;
                } else {
                    m_SelectionType = SelectionType::State;
                    m_SelectedStateId = stateId;
                    m_IsDraggingNode = true;
                }
                clickedOnState = true;
            }
            if (leftDoubleClicked) {
                m_SelectionType = SelectionType::State;
                m_SelectedStateId = stateId;
                m_IsRenaming = true;
                strncpy(m_RenameBuffer, stateId.c_str(), sizeof(m_RenameBuffer) - 1);
                m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
                clickedOnState = true;
            }
            if (rightClicked) {
                m_ShowContextMenu = true;
                m_ContextMenuPos = mousePos;
                m_ContextMenuStateId = stateId;
                m_SelectionType = SelectionType::State;
                m_SelectedStateId = stateId;
                clickedOnState = true;
            }
        }
    }

    // Handle click detection for transitions (only if we didn't click on a state)
    bool clickedOnTransition = false;
    if (!clickedOnState && leftClicked) {
        auto& transitions = m_Controller->GetTransitions();
        for (size_t i = 0; i < transitions.size(); i++) {
            const auto& trans = transitions[i];
            ImVec2 fromCenter, toCenter;
            bool isFromAnyState = trans.anyState;

            if (isFromAnyState) {
                fromCenter = GetAnyStateNodeCenter();
            } else {
                fromCenter = GetStateNodeCenter(trans.from);
            }
            toCenter = GetStateNodeCenter(trans.to);

            if ((fromCenter.x != 0 || fromCenter.y != 0) && (toCenter.x != 0 || toCenter.y != 0)) {
                // Calculate direction (same as DrawTransitionArrow)
                float dx = toCenter.x - fromCenter.x;
                float dy = toCenter.y - fromCenter.y;
                float len = sqrtf(dx * dx + dy * dy);
                if (len < 0.001f) continue;

                dx /= len;
                dy /= len;

                // Perpendicular direction (must match DrawTransitionArrow)
                float perpX = -dy;
                float perpY = dx;
                // Use consistent perpendicular regardless of direction
                if (perpY < 0 || (perpY == 0 && perpX < 0)) {
                    perpX = -perpX;
                    perpY = -perpY;
                }

                // Check for bidirectional transitions - same logic as DrawTransitions
                float perpOffset = 0.0f;
                if (!isFromAnyState) {
                    for (size_t j = 0; j < transitions.size(); j++) {
                        if (i != j && !transitions[j].anyState &&
                            transitions[j].from == trans.to && transitions[j].to == trans.from) {
                            // Must match DrawTransitions offset calculation
                            perpOffset = (trans.from < trans.to) ? 6.0f : -6.0f;
                            break;
                        }
                    }
                }

                // Apply same offset as DrawTransitionArrow to get actual visible line
                float halfWidth = NODE_WIDTH * m_ViewZoom * 0.5f;
                float halfHeight = NODE_HEIGHT * m_ViewZoom * 0.5f;
                float edgeOffsetX = (fabsf(dx) > 0.001f) ? halfWidth / fabsf(dx) : 9999.0f;
                float edgeOffsetY = (fabsf(dy) > 0.001f) ? halfHeight / fabsf(dy) : 9999.0f;
                float edgeOffset = fminf(edgeOffsetX, edgeOffsetY) + 3.0f;
                ImVec2 start(fromCenter.x + dx * edgeOffset, fromCenter.y + dy * edgeOffset);
                ImVec2 end(toCenter.x - dx * edgeOffset, toCenter.y - dy * edgeOffset);
                start.x += perpX * perpOffset;
                start.y += perpY * perpOffset;
                end.x += perpX * perpOffset;
                end.y += perpY * perpOffset;

                // Calculate point-to-line-segment distance using the actual visible line
                float lineDx = end.x - start.x;
                float lineDy = end.y - start.y;
                float lengthSq = lineDx * lineDx + lineDy * lineDy;

                float dist;
                if (lengthSq < 0.001f) {
                    dist = sqrtf(powf(mousePos.x - start.x, 2) + powf(mousePos.y - start.y, 2));
                } else {
                    float t = ((mousePos.x - start.x) * lineDx + (mousePos.y - start.y) * lineDy) / lengthSq;
                    t = std::clamp(t, 0.0f, 1.0f);
                    float projX = start.x + t * lineDx;
                    float projY = start.y + t * lineDy;
                    dist = sqrtf(powf(mousePos.x - projX, 2) + powf(mousePos.y - projY, 2));
                }

                if (dist < 10.0f) {  // Slightly smaller threshold for more precise selection
                    m_SelectionType = SelectionType::Transition;
                    m_SelectedTransitionIndex = i;
                    clickedOnTransition = true;
                    break;
                }
            }
        }
    }

    DrawStates();  // Just draws, no click handling
    DrawTransitions();  // Just draws, no click handling

    // Draw transition creation line
    if (m_IsCreatingTransition) {
        DrawTransitionCreationLine();
    }

    drawList->PopClipRect();

    // Handle canvas-level input (pan/zoom) only when hovered and no specific item clicked
    // Pass whether we clicked on something to prevent unwanted deselection
    if (isCanvasHovered || isCanvasActive) {
        HandleCanvasInput(clickedOnState || clickedOnTransition);
    }
    HandleContextMenu();
}

void AnimatorEditorWindow::DrawGrid()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float gridSize = 50.0f * m_ViewZoom;

    // Calculate grid offset based on view
    float offsetX = fmodf(m_ViewOffset.x * m_ViewZoom + m_CanvasSize.x * 0.5f, gridSize);
    float offsetY = fmodf(m_ViewOffset.y * m_ViewZoom + m_CanvasSize.y * 0.5f, gridSize);

    ImU32 gridColor = IM_COL32(50, 50, 50, 255);

    // Vertical lines
    for (float x = offsetX; x < m_CanvasSize.x; x += gridSize) {
        drawList->AddLine(
            ImVec2(m_CanvasPos.x + x, m_CanvasPos.y),
            ImVec2(m_CanvasPos.x + x, m_CanvasPos.y + m_CanvasSize.y),
            gridColor);
    }

    // Horizontal lines
    for (float y = offsetY; y < m_CanvasSize.y; y += gridSize) {
        drawList->AddLine(
            ImVec2(m_CanvasPos.x, m_CanvasPos.y + y),
            ImVec2(m_CanvasPos.x + m_CanvasSize.x, m_CanvasPos.y + y),
            gridColor);
    }
}

void AnimatorEditorWindow::DrawStates()
{
    for (auto& [stateId, config] : m_Controller->GetStates()) {
        DrawStateNode(stateId, config);
    }
}

void AnimatorEditorWindow::DrawStateNode(const std::string& stateId, AnimStateConfig& config)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 worldPos(config.nodePosition.x, config.nodePosition.y);
    ImVec2 screenPos = WorldToScreen(worldPos);
    ImVec2 nodeSize(NODE_WIDTH * m_ViewZoom, NODE_HEIGHT * m_ViewZoom);

    bool isSelected = (m_SelectionType == SelectionType::State && m_SelectedStateId == stateId);
    bool isEntry = (m_Controller->GetEntryState() == stateId);

    // Node background
    ImU32 bgColor = GetStateColor(isSelected, isEntry);
    drawList->AddRectFilled(screenPos,
        ImVec2(screenPos.x + nodeSize.x, screenPos.y + nodeSize.y),
        bgColor, NODE_ROUNDING * m_ViewZoom);

    // Node border
    ImU32 borderColor = isSelected ? IM_COL32(255, 200, 50, 255) : IM_COL32(100, 100, 100, 255);
    drawList->AddRect(screenPos,
        ImVec2(screenPos.x + nodeSize.x, screenPos.y + nodeSize.y),
        borderColor, NODE_ROUNDING * m_ViewZoom, 0, 2.0f);

    // Entry indicator
    if (isEntry) {
        ImVec2 indicatorPos(screenPos.x - 10 * m_ViewZoom, screenPos.y + nodeSize.y * 0.5f);
        drawList->AddTriangleFilled(
            ImVec2(indicatorPos.x - 8 * m_ViewZoom, indicatorPos.y - 6 * m_ViewZoom),
            ImVec2(indicatorPos.x - 8 * m_ViewZoom, indicatorPos.y + 6 * m_ViewZoom),
            ImVec2(indicatorPos.x, indicatorPos.y),
            IM_COL32(255, 150, 50, 255));
    }

    // State name - centered horizontally
    ImVec2 stateTextSize = ImGui::CalcTextSize(stateId.c_str());
    ImVec2 textPos(
        screenPos.x + (nodeSize.x - stateTextSize.x) * 0.5f,
        screenPos.y + nodeSize.y * 0.5f - stateTextSize.y - 1 * m_ViewZoom
    );
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), stateId.c_str());

    // Clip name indicator - centered horizontally
    std::string clipText;
    auto& clipPaths = m_Controller->GetClipPaths();
    if (config.clipIndex < clipPaths.size()) {
        clipText = GetClipDisplayName(clipPaths[config.clipIndex]);
    } else {
        clipText = "(No Clip)";
    }
    ImVec2 clipTextSize = ImGui::CalcTextSize(clipText.c_str());
    ImVec2 clipTextPos(
        screenPos.x + (nodeSize.x - clipTextSize.x) * 0.5f,
        screenPos.y + nodeSize.y * 0.5f + 1 * m_ViewZoom
    );
    drawList->AddText(clipTextPos, IM_COL32(180, 180, 180, 255), clipText.c_str());

    // Note: Click handling moved to DrawNodeGraph for proper event ordering

    // Handle dragging
    if (m_IsDraggingNode && m_SelectedStateId == stateId && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        config.nodePosition.x += delta.x / m_ViewZoom;
        config.nodePosition.y += delta.y / m_ViewZoom;
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        m_HasUnsavedChanges = true;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_IsDraggingNode = false;
    }
}

void AnimatorEditorWindow::DrawEntryNode()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 entryPos = m_Controller->GetEntryNodePosition();
    ImVec2 screenPos = WorldToScreen(ImVec2(entryPos.x, entryPos.y));
    float nodeSize = 30.0f * m_ViewZoom;

    bool isSelected = (m_SelectionType == SelectionType::EntryNode);

    // Draw circle
    ImU32 color = GetEntryNodeColor();
    drawList->AddCircleFilled(screenPos, nodeSize * 0.5f, color);

    if (isSelected) {
        drawList->AddCircle(screenPos, nodeSize * 0.5f + 2, IM_COL32(255, 200, 50, 255), 0, 2.0f);
    }

    // Draw "Entry" text - centered below the circle
    const char* entryText = "Entry";
    ImVec2 entryTextSize = ImGui::CalcTextSize(entryText);
    drawList->AddText(
        ImVec2(screenPos.x - entryTextSize.x * 0.5f, screenPos.y + nodeSize * 0.5f + 5),
        IM_COL32(200, 200, 200, 255), entryText);

    // Draw arrow to entry state
    if (!m_Controller->GetEntryState().empty()) {
        ImVec2 entryStateCenter = GetStateNodeCenter(m_Controller->GetEntryState());
        if (entryStateCenter.x != 0 || entryStateCenter.y != 0) {
            DrawTransitionArrow(screenPos, entryStateCenter, false);
        }
    }

    // Handle click
    ImVec2 mousePos = ImGui::GetMousePos();
    float dist = sqrtf(powf(mousePos.x - screenPos.x, 2) + powf(mousePos.y - screenPos.y, 2));
    if (dist < nodeSize * 0.5f) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_SelectionType = SelectionType::EntryNode;
        }
    }
}

void AnimatorEditorWindow::DrawAnyStateNode()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 anyPos = m_Controller->GetAnyStatePosition();
    ImVec2 screenPos = WorldToScreen(ImVec2(anyPos.x, anyPos.y));
    ImVec2 nodeSize(80.0f * m_ViewZoom, 30.0f * m_ViewZoom);

    bool isSelected = (m_SelectionType == SelectionType::AnyStateNode);

    // Draw rounded rect
    ImU32 color = GetAnyStateNodeColor();
    drawList->AddRectFilled(
        ImVec2(screenPos.x - nodeSize.x * 0.5f, screenPos.y - nodeSize.y * 0.5f),
        ImVec2(screenPos.x + nodeSize.x * 0.5f, screenPos.y + nodeSize.y * 0.5f),
        color, NODE_ROUNDING * m_ViewZoom);

    if (isSelected) {
        drawList->AddRect(
            ImVec2(screenPos.x - nodeSize.x * 0.5f, screenPos.y - nodeSize.y * 0.5f),
            ImVec2(screenPos.x + nodeSize.x * 0.5f, screenPos.y + nodeSize.y * 0.5f),
            IM_COL32(255, 200, 50, 255), NODE_ROUNDING * m_ViewZoom, 0, 2.0f);
    }

    // Draw text - centered
    const char* anyStateText = "Any State";
    ImVec2 anyTextSize = ImGui::CalcTextSize(anyStateText);
    drawList->AddText(
        ImVec2(screenPos.x - anyTextSize.x * 0.5f, screenPos.y - anyTextSize.y * 0.5f),
        IM_COL32(255, 255, 255, 255), anyStateText);

    // Handle click - start transition from any state
    ImVec2 mousePos = ImGui::GetMousePos();
    if (mousePos.x >= screenPos.x - nodeSize.x * 0.5f && mousePos.x <= screenPos.x + nodeSize.x * 0.5f &&
        mousePos.y >= screenPos.y - nodeSize.y * 0.5f && mousePos.y <= screenPos.y + nodeSize.y * 0.5f) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_SelectionType = SelectionType::AnyStateNode;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_ShowContextMenu = true;
            m_ContextMenuPos = mousePos;
            m_ContextMenuStateId = "__any_state__";
        }
    }
}

void AnimatorEditorWindow::DrawTransitions()
{
    // Note: Click handling moved to DrawNodeGraph for proper event ordering
    auto& transitions = m_Controller->GetTransitions();

    for (size_t i = 0; i < transitions.size(); i++) {
        const auto& trans = transitions[i];
        bool isSelected = (m_SelectionType == SelectionType::Transition && m_SelectedTransitionIndex == i);

        ImVec2 fromPos, toPos;
        bool isFromAnyState = trans.anyState;

        if (isFromAnyState) {
            fromPos = GetAnyStateNodeCenter();
        } else {
            fromPos = GetStateNodeCenter(trans.from);
        }
        toPos = GetStateNodeCenter(trans.to);

        if ((fromPos.x != 0 || fromPos.y != 0) && (toPos.x != 0 || toPos.y != 0)) {
            // Check for bidirectional transitions - offset if reverse exists
            float perpOffset = 0.0f;
            if (!isFromAnyState) {
                for (size_t j = 0; j < transitions.size(); j++) {
                    if (i != j && !transitions[j].anyState &&
                        transitions[j].from == trans.to && transitions[j].to == trans.from) {
                        // There's a reverse transition - offset this one
                        // Use consistent side based on alphabetical order of state names
                        // 6px each side = 12px total separation (Unity-style parallel lines)
                        perpOffset = (trans.from < trans.to) ? 6.0f : -6.0f;
                        break;
                    }
                }
            }
            DrawTransitionArrow(fromPos, toPos, isSelected, isFromAnyState, perpOffset);
        }
    }
}

void AnimatorEditorWindow::DrawTransitionArrow(const ImVec2& from, const ImVec2& to, bool isSelected, bool isFromAnyState, float perpOffset)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImU32 color = GetTransitionColor(isSelected);
    if (isFromAnyState) {
        color = IM_COL32(150, 200, 255, 255);
    }

    // Calculate direction from center to center
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    dx /= len;
    dy /= len;

    // Perpendicular direction (rotate 90 degrees CCW)
    float perpX = -dy;
    float perpY = dx;

    // For bidirectional: use CONSISTENT perpendicular regardless of direction
    // Always use perpendicular that points in positive Y (up) or positive X (right) direction
    if (perpY < 0 || (perpY == 0 && perpX < 0)) {
        perpX = -perpX;
        perpY = -perpY;
    }

    // Calculate edge offset based on direction (use width for horizontal, height for vertical)
    // This ensures arrows start from the correct edge of the rectangular node
    float halfWidth = NODE_WIDTH * m_ViewZoom * 0.5f;
    float halfHeight = NODE_HEIGHT * m_ViewZoom * 0.5f;

    // Calculate how far to move along direction to reach node edge
    // For a rectangle, we need to find where the ray intersects the edge
    float edgeOffsetX = (fabsf(dx) > 0.001f) ? halfWidth / fabsf(dx) : 9999.0f;
    float edgeOffsetY = (fabsf(dy) > 0.001f) ? halfHeight / fabsf(dy) : 9999.0f;
    float edgeOffset = fminf(edgeOffsetX, edgeOffsetY) + 3.0f; // Small gap from edge

    // Calculate start and end at node edges
    ImVec2 start(from.x + dx * edgeOffset, from.y + dy * edgeOffset);
    ImVec2 end(to.x - dx * edgeOffset, to.y - dy * edgeOffset);

    // Apply perpendicular offset for bidirectional transitions (offset the whole line)
    start.x += perpX * perpOffset;
    start.y += perpY * perpOffset;
    end.x += perpX * perpOffset;
    end.y += perpY * perpOffset;

    // Draw line
    float thickness = isSelected ? 3.0f : 2.0f;
    drawList->AddLine(start, end, color, thickness);

    // Draw arrowhead
    float arrowSize = 10.0f * m_ViewZoom;
    ImVec2 arrowP1(end.x - dx * arrowSize - dy * arrowSize * 0.5f,
                   end.y - dy * arrowSize + dx * arrowSize * 0.5f);
    ImVec2 arrowP2(end.x - dx * arrowSize + dy * arrowSize * 0.5f,
                   end.y - dy * arrowSize - dx * arrowSize * 0.5f);
    drawList->AddTriangleFilled(end, arrowP1, arrowP2, color);
}

void AnimatorEditorWindow::DrawTransitionCreationLine()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 startPos;
    if (m_TransitionFromState == "__any_state__") {
        startPos = GetAnyStateNodeCenter();
    } else {
        startPos = GetStateNodeCenter(m_TransitionFromState);
    }

    ImVec2 endPos = ImGui::GetMousePos();

    drawList->AddLine(startPos, endPos, IM_COL32(255, 255, 100, 200), 2.0f);
}

void AnimatorEditorWindow::DrawInspectorPanel()
{
    ImGui::Text(ICON_FA_CIRCLE_INFO " Inspector");
    ImGui::Separator();

    switch (m_SelectionType) {
        case SelectionType::State:
            DrawStateInspector();
            break;
        case SelectionType::Transition:
            DrawTransitionInspector();
            break;
        case SelectionType::EntryNode:
            ImGui::Text("Entry Node");
            ImGui::TextWrapped("The entry node defines which state the animator starts in.");
            break;
        case SelectionType::AnyStateNode:
            ImGui::Text("Any State");
            ImGui::TextWrapped("Transitions from Any State can trigger from any current state.");
            ImGui::Separator();
            if (ImGui::Button("Create Transition")) {
                m_IsCreatingTransition = true;
                m_TransitionFromState = "__any_state__";
            }
            break;
        default:
            ImGui::TextDisabled("Select a state or transition to inspect");
            break;
    }
}

void AnimatorEditorWindow::DrawStateInspector()
{
    auto* config = m_Controller->GetStates().count(m_SelectedStateId)
        ? &m_Controller->GetStates()[m_SelectedStateId] : nullptr;

    if (!config) {
        ImGui::TextDisabled("State not found");
        return;
    }

    // State name - editable with rename functionality
    if (m_IsRenaming && m_SelectionType == SelectionType::State) {
        ImGui::Text("State Name:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##RenameState", m_RenameBuffer, sizeof(m_RenameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            // Apply rename on Enter
            std::string newName(m_RenameBuffer);
            if (!newName.empty() && newName != m_SelectedStateId) {
                RenameState(m_SelectedStateId, newName);
            }
            m_IsRenaming = false;
        }
        // Cancel on Escape
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_IsRenaming = false;
        }
        // Set focus on first frame
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_CHECK "##ApplyRename")) {
            std::string newName(m_RenameBuffer);
            if (!newName.empty() && newName != m_SelectedStateId) {
                RenameState(m_SelectedStateId, newName);
            }
            m_IsRenaming = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Apply rename (Enter)");
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK "##CancelRename")) {
            m_IsRenaming = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Cancel (Escape)");
        }
    } else {
        ImGui::Text("State: %s", m_SelectedStateId.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_PEN "##RenameBtn")) {
            m_IsRenaming = true;
            strncpy(m_RenameBuffer, m_SelectedStateId.c_str(), sizeof(m_RenameBuffer) - 1);
            m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Rename state");
        }
    }
    ImGui::Separator();

    // Animation Clip selection (Unity-style: single field with browse button)
    ImGui::Text("Motion");

    auto& clipPaths = m_Controller->GetClipPaths();

    // Get current clip name
    std::string currentClipName = "(None)";
    std::string currentClipPath = "";
    if (config->clipIndex < clipPaths.size()) {
        currentClipName = GetClipDisplayName(clipPaths[config->clipIndex]);
        currentClipPath = clipPaths[config->clipIndex];
    }

    // Display current clip as a selectable field (like Unity's object field)
    ImGui::SetNextItemWidth(-30);  // Leave space for browse button
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_Button));
    ImGui::InputText("##ClipField", &currentClipName[0], currentClipName.size() + 1, ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && !currentClipPath.empty()) {
        ImGui::SetTooltip("%s", currentClipPath.c_str());
    }

    // Browse button
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_CIRCLE_DOT "##BrowseClip")) {
        std::string newPath = OpenAnimationFileDialog();
        if (!newPath.empty()) {
            // Normalize the path to be relative from "Resources" for cross-machine compatibility
            std::string normalizedPath = newPath;
            size_t resPos = newPath.find("Resources");
            if (resPos != std::string::npos) {
                normalizedPath = newPath.substr(resPos);
            }
            // Also normalize separators to forward slashes
            std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

            // Check if this clip already exists in clipPaths
            auto it = std::find(clipPaths.begin(), clipPaths.end(), normalizedPath);
            if (it != clipPaths.end()) {
                // Use existing clip
                config->clipIndex = std::distance(clipPaths.begin(), it);
            } else {
                // Add new clip
                clipPaths.push_back(normalizedPath);
                config->clipIndex = clipPaths.size() - 1;
            }

            m_HasUnsavedChanges = true;

            // Apply changes - this will also clean up unused clips
            ApplyToAnimationComponent();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Select animation clip");
    }

    // Loop
    if (ImGui::Checkbox("Loop", &config->loop)) {
        m_HasUnsavedChanges = true;
        ApplyToAnimationComponent();
    }

    // Speed
    ImGui::Text("Speed");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputFloat("##Speed", &config->speed, 0.1f, 1.0f, "%.3f")) {
        if (config->speed < 0.0f) config->speed = 0.0f;  // Prevent negative speed
        m_HasUnsavedChanges = true;
        ApplyToAnimationComponent();
    }

    ImGui::Separator();

    // Set as entry button
    if (m_Controller->GetEntryState() != m_SelectedStateId) {
        if (ImGui::Button(ICON_FA_PLAY " Set as Entry State")) {
            SetAsEntryState(m_SelectedStateId);
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), ICON_FA_CHECK " This is the entry state");
    }

    // Create transition button
    if (ImGui::Button(ICON_FA_ARROW_RIGHT " Create Transition From Here")) {
        m_IsCreatingTransition = true;
        m_TransitionFromState = m_SelectedStateId;
    }

    ImGui::Separator();

    // List transitions from this state
    ImGui::Text("Outgoing Transitions:");
    auto& transitions = m_Controller->GetTransitions();
    for (size_t i = 0; i < transitions.size(); i++) {
        if (transitions[i].from == m_SelectedStateId) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(("-> " + transitions[i].to).c_str(),
                m_SelectionType == SelectionType::Transition && m_SelectedTransitionIndex == i)) {
                m_SelectionType = SelectionType::Transition;
                m_SelectedTransitionIndex = i;
            }
            ImGui::PopID();
        }
    }
}

void AnimatorEditorWindow::DrawTransitionInspector()
{
    if (m_SelectedTransitionIndex >= m_Controller->GetTransitions().size()) {
        ImGui::TextDisabled("Transition not found");
        return;
    }

    auto& trans = m_Controller->GetTransitions()[m_SelectedTransitionIndex];

    std::string fromLabel = trans.anyState ? "Any State" : trans.from;
    ImGui::Text("Transition: %s -> %s", fromLabel.c_str(), trans.to.c_str());
    ImGui::Separator();

    // Has exit time
    if (ImGui::Checkbox("Has Exit Time", &trans.hasExitTime)) {
        m_HasUnsavedChanges = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Wait for animation to reach exit time before transitioning");
    }

    if (trans.hasExitTime) {
        if (ImGui::SliderFloat("Exit Time", &trans.exitTime, 0.0f, 1.0f, "%.2f")) {
            m_HasUnsavedChanges = true;
        }
    }

    // Transition duration
    if (ImGui::SliderFloat("Duration", &trans.transitionDuration, 0.0f, 1.0f, "%.2f")) {
        m_HasUnsavedChanges = true;
    }

    ImGui::Separator();
    ImGui::Text("Conditions:");

    DrawConditionEditor(trans);

    ImGui::Separator();
    if (ImGui::Button(ICON_FA_TRASH " Delete Transition")) {
        DeleteSelectedTransition();
    }
}

void AnimatorEditorWindow::DrawConditionEditor(AnimTransition& transition)
{
    auto& params = m_Controller->GetParameters();
    auto& conditions = transition.conditions;

    // Existing conditions
    for (size_t i = 0; i < conditions.size(); i++) {
        auto& cond = conditions[i];
        ImGui::PushID(static_cast<int>(i));

        // Parameter selector
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("##Param", cond.paramName.c_str())) {
            for (const auto& param : params) {
                if (ImGui::Selectable(param.name.c_str(), param.name == cond.paramName)) {
                    cond.paramName = param.name;
                    m_HasUnsavedChanges = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // Get param type
        AnimParamType paramType = AnimParamType::Bool;
        for (const auto& param : params) {
            if (param.name == cond.paramName) {
                paramType = param.type;
                break;
            }
        }

        // Mode selector - different UI based on parameter type
        if (paramType == AnimParamType::Trigger) {
            // Trigger: just show "fired"
            ImGui::TextDisabled("fired");
            cond.mode = AnimConditionMode::TriggerFired;
        } else if (paramType == AnimParamType::Bool) {
            // Bool: Unity-style dropdown with just "true" / "false"
            cond.mode = AnimConditionMode::Equals; // Bool always uses equals
            ImGui::SetNextItemWidth(60);
            const char* boolOptions[] = { "true", "false" };
            int currentBool = cond.threshold > 0.5f ? 0 : 1;
            if (ImGui::BeginCombo("##BoolValue", boolOptions[currentBool])) {
                if (ImGui::Selectable("true", currentBool == 0)) {
                    cond.threshold = 1.0f;
                    m_HasUnsavedChanges = true;
                }
                if (ImGui::Selectable("false", currentBool == 1)) {
                    cond.threshold = 0.0f;
                    m_HasUnsavedChanges = true;
                }
                ImGui::EndCombo();
            }
        } else if (paramType == AnimParamType::Int) {
            // Int: Unity shows Greater, Less, Equals, NotEqual
            const char* intModeNames[] = { "Greater", "Less", "Equals", "NotEqual" };
            AnimConditionMode intModes[] = {
                AnimConditionMode::Greater, AnimConditionMode::Less,
                AnimConditionMode::Equals, AnimConditionMode::NotEquals
            };
            int currentMode = 0;
            for (int m = 0; m < 4; m++) {
                if (intModes[m] == cond.mode) { currentMode = m; break; }
            }
            ImGui::SetNextItemWidth(75);
            if (ImGui::BeginCombo("##Mode", intModeNames[currentMode])) {
                for (int m = 0; m < 4; m++) {
                    if (ImGui::Selectable(intModeNames[m], currentMode == m)) {
                        cond.mode = intModes[m];
                        m_HasUnsavedChanges = true;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();

            // Int value input
            ImGui::SetNextItemWidth(60);
            int intVal = static_cast<int>(cond.threshold);
            if (ImGui::InputInt("##Threshold", &intVal, 0, 0)) {
                cond.threshold = static_cast<float>(intVal);
                m_HasUnsavedChanges = true;
            }
        } else {
            // Float: Unity only shows Greater, Less (no equals for float comparison)
            const char* floatModeNames[] = { "Greater", "Less" };
            AnimConditionMode floatModes[] = {
                AnimConditionMode::Greater, AnimConditionMode::Less
            };
            int currentMode = 0;
            for (int m = 0; m < 2; m++) {
                if (floatModes[m] == cond.mode) { currentMode = m; break; }
            }
            ImGui::SetNextItemWidth(65);
            if (ImGui::BeginCombo("##Mode", floatModeNames[currentMode])) {
                for (int m = 0; m < 2; m++) {
                    if (ImGui::Selectable(floatModeNames[m], currentMode == m)) {
                        cond.mode = floatModes[m];
                        m_HasUnsavedChanges = true;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();

            // Float value input
            ImGui::SetNextItemWidth(60);
            if (ImGui::InputFloat("##Threshold", &cond.threshold, 0.0f, 0.0f, "%.2f")) {
                m_HasUnsavedChanges = true;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK "##RemoveCond")) {
            conditions.erase(conditions.begin() + i);
            m_HasUnsavedChanges = true;
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    // Add condition button
    if (!params.empty()) {
        if (ImGui::Button(ICON_FA_PLUS " Add Condition")) {
            AnimCondition newCond;
            newCond.paramName = params[0].name;
            newCond.mode = AnimConditionMode::Equals;
            newCond.threshold = 0.0f;
            conditions.push_back(newCond);
            m_HasUnsavedChanges = true;
        }
    } else {
        ImGui::TextDisabled("Add parameters first");
    }
}

// Coordinate helpers
ImVec2 AnimatorEditorWindow::WorldToScreen(const ImVec2& worldPos) const
{
    return ImVec2(
        m_CanvasPos.x + m_CanvasSize.x * 0.5f + (worldPos.x + m_ViewOffset.x) * m_ViewZoom,
        m_CanvasPos.y + m_CanvasSize.y * 0.5f + (worldPos.y + m_ViewOffset.y) * m_ViewZoom
    );
}

ImVec2 AnimatorEditorWindow::ScreenToWorld(const ImVec2& screenPos) const
{
    return ImVec2(
        (screenPos.x - m_CanvasPos.x - m_CanvasSize.x * 0.5f) / m_ViewZoom - m_ViewOffset.x,
        (screenPos.y - m_CanvasPos.y - m_CanvasSize.y * 0.5f) / m_ViewZoom - m_ViewOffset.y
    );
}

ImVec2 AnimatorEditorWindow::GetStateNodeCenter(const std::string& stateId) const
{
    auto& states = m_Controller->GetStates();
    auto it = states.find(stateId);
    if (it == states.end()) return ImVec2(0, 0);

    const auto& config = it->second;
    ImVec2 worldCenter(config.nodePosition.x + NODE_WIDTH * 0.5f,
                       config.nodePosition.y + NODE_HEIGHT * 0.5f);
    return WorldToScreen(worldCenter);
}

ImVec2 AnimatorEditorWindow::GetEntryNodeCenter() const
{
    glm::vec2 pos = m_Controller->GetEntryNodePosition();
    return WorldToScreen(ImVec2(pos.x, pos.y));
}

ImVec2 AnimatorEditorWindow::GetAnyStateNodeCenter() const
{
    glm::vec2 pos = m_Controller->GetAnyStatePosition();
    return WorldToScreen(ImVec2(pos.x, pos.y));
}

// Input handling
void AnimatorEditorWindow::HandleCanvasInput(bool clickedOnItem)
{
    ImVec2 mousePos = ImGui::GetMousePos();

    // Check if mouse is in canvas
    bool inCanvas = mousePos.x >= m_CanvasPos.x && mousePos.x <= m_CanvasPos.x + m_CanvasSize.x &&
                    mousePos.y >= m_CanvasPos.y && mousePos.y <= m_CanvasPos.y + m_CanvasSize.y;

    if (!inCanvas) return;

    // Pan with middle mouse
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
        m_ViewOffset.x += delta.x / m_ViewZoom;
        m_ViewOffset.y += delta.y / m_ViewZoom;
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
    }

    // Zoom with scroll
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
        float oldZoom = m_ViewZoom;
        m_ViewZoom = std::clamp(m_ViewZoom + wheel * 0.1f, 0.25f, 2.0f);

        // Zoom towards mouse position
        if (oldZoom != m_ViewZoom) {
            ImVec2 worldMouse = ScreenToWorld(mousePos);
            m_ViewOffset.x += worldMouse.x * (1.0f - m_ViewZoom / oldZoom);
            m_ViewOffset.y += worldMouse.y * (1.0f - m_ViewZoom / oldZoom);
        }
    }

    // Cancel transition creation with right click or escape
    if (m_IsCreatingTransition) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_IsCreatingTransition = false;
        }
    }

    // Deselect on empty click (only if we didn't click on a state or transition)
    if (!clickedOnItem && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_IsDraggingNode) {
        auto stateAtPos = GetStateAtPosition(mousePos);
        if (!stateAtPos.has_value()) {
            // Check if not clicking on special nodes
            ImVec2 entryCenter = GetEntryNodeCenter();
            ImVec2 anyCenter = GetAnyStateNodeCenter();
            float distEntry = sqrtf(powf(mousePos.x - entryCenter.x, 2) + powf(mousePos.y - entryCenter.y, 2));
            float distAny = sqrtf(powf(mousePos.x - anyCenter.x, 2) + powf(mousePos.y - anyCenter.y, 2));

            if (distEntry > 20 && distAny > 50) {
                m_SelectionType = SelectionType::None;
            }
        }
    }

    // Right-click context menu on empty space
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        auto stateAtPos = GetStateAtPosition(mousePos);
        if (!stateAtPos.has_value()) {
            m_ShowContextMenu = true;
            m_ContextMenuPos = mousePos;
            m_ContextMenuStateId.clear();
        }
    }
}

void AnimatorEditorWindow::HandleContextMenu()
{
    if (m_ShowContextMenu) {
        ImGui::OpenPopup("NodeGraphContextMenu");
        m_ShowContextMenu = false;
    }

    if (ImGui::BeginPopup("NodeGraphContextMenu")) {
        if (m_ContextMenuStateId.empty()) {
            // Empty space menu
            if (ImGui::MenuItem(ICON_FA_PLUS " Create State")) {
                CreateNewState(ScreenToWorld(m_ContextMenuPos));
            }
        } else if (m_ContextMenuStateId == "__any_state__") {
            // Any state menu
            if (ImGui::MenuItem(ICON_FA_ARROW_RIGHT " Make Transition")) {
                m_IsCreatingTransition = true;
                m_TransitionFromState = "__any_state__";
            }
        } else {
            // State menu
            if (ImGui::MenuItem(ICON_FA_ARROW_RIGHT " Make Transition")) {
                m_IsCreatingTransition = true;
                m_TransitionFromState = m_ContextMenuStateId;
            }
            if (ImGui::MenuItem(ICON_FA_PLAY " Set as Entry State")) {
                SetAsEntryState(m_ContextMenuStateId);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
                m_SelectedStateId = m_ContextMenuStateId;
                m_SelectionType = SelectionType::State;
                m_IsRenaming = true;
                strncpy(m_RenameBuffer, m_ContextMenuStateId.c_str(), sizeof(m_RenameBuffer) - 1);
                m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
            }
            if (ImGui::MenuItem(ICON_FA_COPY " Duplicate")) {
                DuplicateSelectedState();
            }
            if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
                m_SelectedStateId = m_ContextMenuStateId;
                m_SelectionType = SelectionType::State;
                DeleteSelectedState();
            }
        }
        ImGui::EndPopup();
    }
}

void AnimatorEditorWindow::HandleKeyboardShortcuts()
{
    if (!IsOpen()) return;

    // Don't process shortcuts while renaming
    if (m_IsRenaming) return;

    // Ctrl+S to save
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        SaveController();
    }

    // F2 to rename selected state
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
        if (m_SelectionType == SelectionType::State && !m_SelectedStateId.empty()) {
            m_IsRenaming = true;
            strncpy(m_RenameBuffer, m_SelectedStateId.c_str(), sizeof(m_RenameBuffer) - 1);
            m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
        }
    }

    // Delete key
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (m_SelectionType == SelectionType::State) {
            DeleteSelectedState();
        } else if (m_SelectionType == SelectionType::Transition) {
            DeleteSelectedTransition();
        }
    }

    // Escape to deselect or cancel
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (m_IsCreatingTransition) {
            m_IsCreatingTransition = false;
        } else {
            m_SelectionType = SelectionType::None;
        }
    }
}

// State operations
void AnimatorEditorWindow::CreateNewState(const ImVec2& position)
{
    std::string stateName = GenerateUniqueStateName();
    AnimStateConfig config;
    config.nodePosition = glm::vec2(position.x, position.y);
    config.clipIndex = 0;
    config.loop = true;
    config.speed = 1.0f;

    m_Controller->AddState(stateName, config);

    // Set as entry if first state
    if (m_Controller->GetStates().size() == 1) {
        m_Controller->SetEntryState(stateName);
    }

    m_SelectionType = SelectionType::State;
    m_SelectedStateId = stateName;
    m_HasUnsavedChanges = true;
}

void AnimatorEditorWindow::DeleteSelectedState()
{
    if (m_SelectionType != SelectionType::State || m_SelectedStateId.empty()) return;

    m_Controller->RemoveState(m_SelectedStateId);
    m_SelectionType = SelectionType::None;
    m_SelectedStateId.clear();
    m_HasUnsavedChanges = true;
}

void AnimatorEditorWindow::DuplicateSelectedState()
{
    if (m_SelectionType != SelectionType::State || m_SelectedStateId.empty()) return;

    auto& states = m_Controller->GetStates();
    auto it = states.find(m_SelectedStateId);
    if (it == states.end()) return;

    std::string newName = GenerateUniqueStateName(m_SelectedStateId);
    AnimStateConfig newConfig = it->second;
    newConfig.nodePosition.x += 50.0f;
    newConfig.nodePosition.y += 50.0f;

    m_Controller->AddState(newName, newConfig);
    m_SelectedStateId = newName;
    m_HasUnsavedChanges = true;
}

void AnimatorEditorWindow::SetAsEntryState(const std::string& stateId)
{
    m_Controller->SetEntryState(stateId);
    m_HasUnsavedChanges = true;
}

void AnimatorEditorWindow::RenameState(const std::string& oldName, const std::string& newName)
{
    if (oldName.empty() || newName.empty() || oldName == newName) return;

    // Check if new name already exists
    if (m_Controller->HasState(newName)) {
        ENGINE_LOG_WARN("[AnimatorEditor] Cannot rename: state '{}' already exists", newName);
        return;
    }

    auto& states = m_Controller->GetStates();
    auto it = states.find(oldName);
    if (it == states.end()) return;

    // Copy the config
    AnimStateConfig config = it->second;

    // Remove old state
    states.erase(it);

    // Add with new name
    states[newName] = config;

    // Update entry state if needed
    if (m_Controller->GetEntryState() == oldName) {
        m_Controller->SetEntryState(newName);
    }

    // Update transitions
    for (auto& trans : m_Controller->GetTransitions()) {
        if (trans.from == oldName) trans.from = newName;
        if (trans.to == oldName) trans.to = newName;
    }

    // Update selection
    m_SelectedStateId = newName;
    m_HasUnsavedChanges = true;
}

// Transition operations
void AnimatorEditorWindow::CreateTransition(const std::string& fromState, const std::string& toState)
{
    if (fromState == toState) return; // No self-transitions for now

    AnimTransition trans;
    if (fromState == "__any_state__") {
        trans.anyState = true;
        trans.from = "";
    } else {
        trans.anyState = false;
        trans.from = fromState;
    }
    trans.to = toState;

    m_Controller->AddTransition(trans);
    m_SelectionType = SelectionType::Transition;
    m_SelectedTransitionIndex = m_Controller->GetTransitions().size() - 1;
    m_HasUnsavedChanges = true;
}

void AnimatorEditorWindow::DeleteSelectedTransition()
{
    if (m_SelectionType != SelectionType::Transition) return;

    m_Controller->RemoveTransition(m_SelectedTransitionIndex);
    m_SelectionType = SelectionType::None;
    m_HasUnsavedChanges = true;
}

// Parameter operations
void AnimatorEditorWindow::AddParameter(AnimParamType type)
{
    std::string name = GenerateUniqueParamName();
    m_Controller->AddParameter(name, type);
    m_HasUnsavedChanges = true;
}

void AnimatorEditorWindow::DeleteParameter(const std::string& name)
{
    m_Controller->RemoveParameter(name);
    m_HasUnsavedChanges = true;
}

// File operations
void AnimatorEditorWindow::SaveController()
{
    if (m_ControllerFilePath.empty()) {
        SaveControllerAs();
        return;
    }

    // Clean up unused clips before saving
    CleanupUnusedClips();

    if (m_Controller->SaveToFile(m_ControllerFilePath)) {
        m_HasUnsavedChanges = false;
    }
}

void AnimatorEditorWindow::SaveControllerAs()
{
    std::string chosenPath;

#ifdef _WIN32
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool coInitialized = SUCCEEDED(hrCo);

    if (coInitialized) {
        IFileSaveDialog* pFileSave = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileSave));
        if (SUCCEEDED(hr) && pFileSave) {
            const COMDLG_FILTERSPEC fileTypes[] = {
                { L"Animator Controller (*.animator)", L"*.animator" },
                { L"All Files (*.*)", L"*.*" }
            };

            pFileSave->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
            pFileSave->SetDefaultExtension(L"animator");
            pFileSave->SetTitle(L"Save Animator Controller");

            // Set default filename - use existing filename or "New Animator"
            std::wstring defaultName = L"New Animator";
            if (!m_ControllerFilePath.empty()) {
                // Use existing file name if we have one
                std::filesystem::path existingPath(m_ControllerFilePath);
                std::string stemName = existingPath.stem().string();
                defaultName = std::wstring(stemName.begin(), stemName.end());
            }
            pFileSave->SetFileName(defaultName.c_str());

            // Set default folder to Resources/Animations if possible
            std::filesystem::path animPath = std::filesystem::current_path() / "Resources" / "Animations";
            if (std::filesystem::exists(animPath)) {
                IShellItem* pFolder = nullptr;
                std::wstring wPath = animPath.wstring();
                if (SUCCEEDED(SHCreateItemFromParsingName(wPath.c_str(), nullptr, IID_PPV_ARGS(&pFolder)))) {
                    pFileSave->SetDefaultFolder(pFolder);
                    pFolder->Release();
                }
            }

            hr = pFileSave->Show(nullptr);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = nullptr;
                if (SUCCEEDED(pFileSave->GetResult(&pItem)) && pItem) {
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath) {
                        std::filesystem::path p(pszFilePath);
                        chosenPath = p.string();
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileSave->Release();
        }
        CoUninitialize();
    }
#endif

    if (!chosenPath.empty()) {
        // Ensure parent directory exists
        std::filesystem::path filePath(chosenPath);
        std::filesystem::path parentDir = filePath.parent_path();
        if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
            std::filesystem::create_directories(parentDir);
        }

        // Clean up unused clips before saving
        CleanupUnusedClips();

        m_ControllerFilePath = chosenPath;
        if (m_Controller->SaveToFile(m_ControllerFilePath)) {
            m_HasUnsavedChanges = false;
            ENGINE_LOG_INFO("[AnimatorEditor] Saved controller to: {}", m_ControllerFilePath);
        }
    }
}

void AnimatorEditorWindow::LoadController()
{
    std::string chosenPath;

#ifdef _WIN32
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool coInitialized = SUCCEEDED(hrCo);

    if (coInitialized) {
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
        if (SUCCEEDED(hr) && pFileOpen) {
            const COMDLG_FILTERSPEC fileTypes[] = {
                { L"Animator Controller (*.animator)", L"*.animator" },
                { L"All Files (*.*)", L"*.*" }
            };

            pFileOpen->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
            pFileOpen->SetTitle(L"Open Animator Controller");

            DWORD options = 0;
            if (SUCCEEDED(pFileOpen->GetOptions(&options))) {
                pFileOpen->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
            }

            // Set default folder to Resources/Animations if possible
            std::filesystem::path animPath = std::filesystem::current_path() / "Resources" / "Animations";
            if (std::filesystem::exists(animPath)) {
                IShellItem* pFolder = nullptr;
                std::wstring wPath = animPath.wstring();
                if (SUCCEEDED(SHCreateItemFromParsingName(wPath.c_str(), nullptr, IID_PPV_ARGS(&pFolder)))) {
                    pFileOpen->SetDefaultFolder(pFolder);
                    pFolder->Release();
                }
            }

            hr = pFileOpen->Show(nullptr);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = nullptr;
                if (SUCCEEDED(pFileOpen->GetResult(&pItem)) && pItem) {
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath) {
                        std::filesystem::path p(pszFilePath);
                        chosenPath = p.string();
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
#endif

    if (!chosenPath.empty()) {
        OpenController(chosenPath);
    }
}

void AnimatorEditorWindow::ApplyToAnimationComponent()
{
    if (!m_AnimComponent || m_CurrentEntity == 0) return;

    // Remove any clips not used by states
    CleanupUnusedClips();

    // Sync clip paths from controller to component
    const auto& ctrlClipPaths = m_Controller->GetClipPaths();
    bool clipPathsChanged = (m_AnimComponent->clipPaths != ctrlClipPaths);

    m_AnimComponent->clipPaths = ctrlClipPaths;
    m_AnimComponent->clipCount = static_cast<int>(ctrlClipPaths.size());
    // Store GUIDs for cross-machine compatibility
    m_AnimComponent->clipGUIDs.clear();
    for (const auto& clipPath : ctrlClipPaths) {
        GUID_128 guid = AssetManager::GetInstance().GetGUID128FromAssetMeta(clipPath);
        m_AnimComponent->clipGUIDs.push_back(guid);
    }

    // Apply state machine configuration
    AnimationStateMachine* sm = m_AnimComponent->EnsureStateMachine();
    m_Controller->ApplyToStateMachine(sm);

    // If clip paths changed, we need to reload the animation clips
    if (clipPathsChanged) {
        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecs.HasComponent<ModelRenderComponent>(m_CurrentEntity)) {
            auto& modelComp = ecs.GetComponent<ModelRenderComponent>(m_CurrentEntity);
            if (modelComp.model) {
                m_AnimComponent->LoadClipsFromPaths(
                    modelComp.model->GetBoneInfoMap(),
                    modelComp.model->GetBoneCount(),
                    m_CurrentEntity
                );
                Animator* animator = m_AnimComponent->EnsureAnimator();
                modelComp.SetAnimator(animator);
            }
        }
    }

    // Get the current/selected state to determine which clip to play
    std::string stateToPlay = m_SelectedStateId;
    if (stateToPlay.empty()) {
        stateToPlay = sm->GetCurrentState();
        if (stateToPlay.empty()) {
            stateToPlay = sm->GetEntryState();
        }
    }

    const AnimStateConfig* stateConfig = sm->GetState(stateToPlay);
    if (stateConfig && stateConfig->clipIndex < m_AnimComponent->GetClips().size()) {
        // Play the animation for the selected/current state
        m_AnimComponent->PlayClip(stateConfig->clipIndex, stateConfig->loop, m_CurrentEntity);
        m_AnimComponent->SetSpeed(stateConfig->speed);
    }
}

// Utility
std::string AnimatorEditorWindow::GenerateUniqueStateName(const std::string& baseName)
{
    auto& states = m_Controller->GetStates();
    std::string name = baseName;
    int counter = 1;

    while (states.find(name) != states.end()) {
        name = baseName + " " + std::to_string(counter);
        counter++;
    }

    return name;
}

std::string AnimatorEditorWindow::GenerateUniqueParamName(const std::string& baseName)
{
    auto& params = m_Controller->GetParameters();
    std::string name = baseName;
    int counter = 1;

    auto exists = [&params](const std::string& n) {
        for (const auto& p : params) {
            if (p.name == n) return true;
        }
        return false;
    };

    while (exists(name)) {
        name = baseName + " " + std::to_string(counter);
        counter++;
    }

    return name;
}

bool AnimatorEditorWindow::IsPointInNode(const ImVec2& point, const ImVec2& nodePos, const ImVec2& nodeSize) const
{
    return point.x >= nodePos.x && point.x <= nodePos.x + nodeSize.x &&
           point.y >= nodePos.y && point.y <= nodePos.y + nodeSize.y;
}

std::optional<std::string> AnimatorEditorWindow::GetStateAtPosition(const ImVec2& screenPos) const
{
    for (const auto& [stateId, config] : m_Controller->GetStates()) {
        ImVec2 worldPos(config.nodePosition.x, config.nodePosition.y);
        ImVec2 nodeScreenPos = WorldToScreen(worldPos);
        ImVec2 nodeSize(NODE_WIDTH * m_ViewZoom, NODE_HEIGHT * m_ViewZoom);

        if (IsPointInNode(screenPos, nodeScreenPos, nodeSize)) {
            return stateId;
        }
    }
    return std::nullopt;
}

// Colors
ImU32 AnimatorEditorWindow::GetStateColor(bool isSelected, bool isEntry)
{
    if (isEntry) {
        return isSelected ? IM_COL32(255, 180, 100, 255) : IM_COL32(200, 130, 50, 255);
    }
    return isSelected ? IM_COL32(100, 150, 200, 255) : IM_COL32(60, 90, 120, 255);
}

ImU32 AnimatorEditorWindow::GetTransitionColor(bool isSelected)
{
    return isSelected ? IM_COL32(255, 200, 50, 255) : IM_COL32(200, 200, 200, 255);
}

ImU32 AnimatorEditorWindow::GetEntryNodeColor()
{
    return IM_COL32(50, 180, 50, 255);
}

ImU32 AnimatorEditorWindow::GetAnyStateNodeColor()
{
    return IM_COL32(80, 160, 180, 255);
}

std::string AnimatorEditorWindow::GetClipDisplayName(const std::string& path) const
{
    if (path.empty()) return "(None)";

    // Get just the filename without extension
    std::filesystem::path fsPath(path);
    return fsPath.stem().string();
}

void AnimatorEditorWindow::CleanupUnusedClips()
{
    if (!m_Controller) return;

    auto& clipPaths = m_Controller->GetClipPaths();
    auto& states = m_Controller->GetStates();

    if (clipPaths.empty()) return;

    // Collect all clip indices used by states
    std::set<size_t> usedIndices;
    for (const auto& [stateId, config] : states) {
        usedIndices.insert(config.clipIndex);
    }

    // Build new clip paths with only used clips, and create index mapping
    std::vector<std::string> newClipPaths;
    std::map<size_t, size_t> oldToNewIndex;

    for (size_t oldIdx = 0; oldIdx < clipPaths.size(); ++oldIdx) {
        if (usedIndices.count(oldIdx) > 0) {
            oldToNewIndex[oldIdx] = newClipPaths.size();
            newClipPaths.push_back(clipPaths[oldIdx]);
        }
    }

    // If nothing changed, skip
    if (newClipPaths.size() == clipPaths.size()) return;

    // Update all state clip indices to use new mapping
    for (auto& [stateId, config] : states) {
        auto it = oldToNewIndex.find(config.clipIndex);
        if (it != oldToNewIndex.end()) {
            config.clipIndex = it->second;
        } else {
            // Clip was removed (shouldn't happen if we collected used indices correctly)
            config.clipIndex = 0;
        }
    }

    // Replace clip paths with the cleaned-up version
    clipPaths = std::move(newClipPaths);
    m_HasUnsavedChanges = true;
}

std::string AnimatorEditorWindow::OpenAnimationFileDialog()
{
    std::string chosenPath;

#ifdef _WIN32
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool coInitialized = SUCCEEDED(hrCo);

    if (coInitialized) {
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
        if (SUCCEEDED(hr) && pFileOpen) {
            // Filter for common animation file formats
            const COMDLG_FILTERSPEC fileTypes[] = {
                { L"Animation Files", L"*.fbx;*.gltf;*.glb;*.dae;*.anim" },
                { L"FBX Files (*.fbx)", L"*.fbx" },
                { L"GLTF Files (*.gltf;*.glb)", L"*.gltf;*.glb" },
                { L"Collada Files (*.dae)", L"*.dae" },
                { L"All Files (*.*)", L"*.*" }
            };

            pFileOpen->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
            pFileOpen->SetTitle(L"Select Animation File");

            // Set default folder to Resources if possible
            std::filesystem::path resourcesPath = std::filesystem::current_path() / "Resources";
            if (std::filesystem::exists(resourcesPath)) {
                IShellItem* pFolder = nullptr;
                std::wstring wPath = resourcesPath.wstring();
                if (SUCCEEDED(SHCreateItemFromParsingName(wPath.c_str(), nullptr, IID_PPV_ARGS(&pFolder)))) {
                    pFileOpen->SetDefaultFolder(pFolder);
                    pFolder->Release();
                }
            }

            // Require file/path to exist
            DWORD options = 0;
            if (SUCCEEDED(pFileOpen->GetOptions(&options))) {
                pFileOpen->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
            }

            hr = pFileOpen->Show(nullptr);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = nullptr;
                if (SUCCEEDED(pFileOpen->GetResult(&pItem)) && pItem) {
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath) {
                        std::filesystem::path p(pszFilePath);
                        chosenPath = p.string();
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }

        CoUninitialize();
    }
#else
    ENGINE_LOG_INFO("[AnimatorEditorWindow] File dialog not supported on this platform");
#endif

    return chosenPath;
}
