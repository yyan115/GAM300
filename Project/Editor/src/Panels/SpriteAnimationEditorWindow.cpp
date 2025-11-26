#include "Panels/SpriteAnimationEditorWindow.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "EditorComponents.hpp"
#include "SnapshotManager.hpp"
#include "EditorState.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Graphics/Texture.h"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

// Global instance
static SpriteAnimationEditorWindow* g_AnimationEditor = nullptr;

SpriteAnimationEditorWindow* GetSpriteAnimationEditor() {
    if (!g_AnimationEditor) {
        g_AnimationEditor = new SpriteAnimationEditorWindow();
    }
    return g_AnimationEditor;
}

void SpriteAnimationEditorWindow::OpenForEntity(Entity entity, SpriteAnimationComponent* animComponent) {
    m_CurrentEntity = entity;
    m_AnimComponent = animComponent;
    m_EditBuffer = *animComponent;  // Copy for editing
    m_IsOpen = true;
    SetOpen(true);  // Set the base class's isOpen flag
    m_HasUnsavedChanges = false;

    // Reset editor state
    m_EditorState = EditorState();

    // Select first clip if available
    if (!m_EditBuffer.clips.empty()) {
        m_EditorState.selectedClipIndex = 0;
        if (!m_EditBuffer.clips[0].frames.empty()) {
            m_EditorState.selectedFrameIndex = 0;
        }
    }
}

void SpriteAnimationEditorWindow::Close() {
    if (m_HasUnsavedChanges) {
        // Could show a confirmation dialog here
        SaveAnimation();  // Auto-save changes when closing
    }
    m_IsOpen = false;
    SetOpen(false);  // Set the base class's isOpen flag
    m_CurrentEntity = 0;
    m_AnimComponent = nullptr;
}

void SpriteAnimationEditorWindow::OnImGuiRender() {
    if (!m_IsOpen || !m_AnimComponent) return;

    // Create a large window for the animation editor
    ImGui::SetNextWindowSize(ImVec2(1400, 800), ImGuiCond_FirstUseEver);

    std::string windowTitle = ICON_FA_FILM " Sprite Animation Editor - Entity " + std::to_string(m_CurrentEntity);
    if (m_HasUnsavedChanges) {
        windowTitle += " *";
    }

    if (ImGui::Begin(windowTitle.c_str(), &m_IsOpen, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save", "Ctrl+S")) {
                    SaveAnimation();
                }
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Load Animation...")) {
                    // LoadAnimation();
                }
                // Removed Export menu item - no animation files
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_XMARK " Close", "Esc")) {
                    Close();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem(ICON_FA_PLUS " Add Clip")) {
                    AddNewClip();
                }
                if (ImGui::MenuItem(ICON_FA_PLUS " Add Frame")) {
                    if (m_EditorState.selectedClipIndex >= 0) {
                        AddNewFrame(m_EditorState.selectedClipIndex);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_COPY " Duplicate Frame", nullptr, false,
                                   m_EditorState.selectedFrameIndex >= 0)) {
                    if (m_EditorState.selectedClipIndex >= 0 && m_EditorState.selectedFrameIndex >= 0) {
                        DuplicateFrame(m_EditorState.selectedClipIndex, m_EditorState.selectedFrameIndex);
                    }
                }
                if (ImGui::MenuItem(ICON_FA_TRASH " Delete Frame", nullptr, false,
                                   m_EditorState.selectedFrameIndex >= 0)) {
                    if (m_EditorState.selectedClipIndex >= 0 && m_EditorState.selectedFrameIndex >= 0) {
                        DeleteFrame(m_EditorState.selectedClipIndex, m_EditorState.selectedFrameIndex);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show Grid", nullptr, &m_EditorState.showGrid);
                ImGui::MenuItem("Show Frame Bounds", nullptr, &m_EditorState.showFrameBounds);
                ImGui::MenuItem("Enable Onion Skin", nullptr, &m_EditorState.enableOnionSkin);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset View")) {
                    m_EditorState.previewZoom = 1.0f;
                    m_EditorState.previewPan = glm::vec2(0.0f);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Handle shortcuts
        HandleKeyboardShortcuts();

        // Main toolbar
        DrawToolbar();

        ImGui::Separator();

        // Main content area with docking
        ImGui::BeginChild("MainContent", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

        // Left: Properties panel
        ImGui::BeginChild("PropertiesPanel", ImVec2(PROPERTIES_WIDTH, 0), true,
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawPropertiesPanel();
        ImGui::EndChild();

        ImGui::SameLine();

        // Center/Right: Preview and Timeline
        ImGui::BeginChild("PreviewTimelineArea", ImVec2(0, 0), false);

        // Top: Preview
        float previewHeight = ImGui::GetContentRegionAvail().y - TIMELINE_HEIGHT - 10;
        ImGui::BeginChild("PreviewPanel", ImVec2(0, previewHeight), true,
                         ImGuiWindowFlags_NoScrollbar);
        DrawPreviewPanel();
        ImGui::EndChild();

        // Bottom: Timeline
        ImGui::BeginChild("TimelinePanel", ImVec2(0, TIMELINE_HEIGHT), true,
                         ImGuiWindowFlags_HorizontalScrollbar);
        DrawTimeline();
        ImGui::EndChild();

        ImGui::EndChild(); // PreviewTimelineArea
        ImGui::EndChild(); // MainContent
    }
    ImGui::End();

    // Update animation preview if playing
    if (m_EditorState.isPlaying) {
        UpdatePreviewAnimation(ImGui::GetIO().DeltaTime);
    }
}

void SpriteAnimationEditorWindow::DrawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

    // Playback controls
    if (m_EditorState.isPlaying) {
        if (ImGui::Button(ICON_FA_PAUSE)) {
            m_EditorState.isPlaying = false;
        }
    } else {
        if (ImGui::Button(ICON_FA_PLAY)) {
            m_EditorState.isPlaying = true;
        }
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_STOP)) {
        m_EditorState.isPlaying = false;
        m_EditorState.currentTime = 0.0f;
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_BACKWARD_STEP)) {
        // Previous frame
        if (m_EditorState.selectedClipIndex >= 0 && m_EditorState.selectedFrameIndex > 0) {
            m_EditorState.selectedFrameIndex--;
            m_EditorState.currentTime = GetFrameStartTime(
                m_EditBuffer.clips[m_EditorState.selectedClipIndex],
                m_EditorState.selectedFrameIndex);
        }
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FORWARD_STEP)) {
        // Next frame
        if (m_EditorState.selectedClipIndex >= 0) {
            auto& clip = m_EditBuffer.clips[m_EditorState.selectedClipIndex];
            if (m_EditorState.selectedFrameIndex < (int)clip.frames.size() - 1) {
                m_EditorState.selectedFrameIndex++;
                m_EditorState.currentTime = GetFrameStartTime(clip, m_EditorState.selectedFrameIndex);
            }
        }
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("##Speed", &m_EditorState.playbackSpeed, 0.01f, 0.1f, 5.0f, "%.2fx");

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Clip selector
    DrawClipSelector();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Timeline zoom
    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderFloat("##TimelineZoom", &m_EditorState.timelineZoom, 0.1f, 5.0f, "%.1fx")) {
        m_EditorState.timelineZoom = std::max(0.1f, std::min(5.0f, m_EditorState.timelineZoom));
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS_MINUS)) {
        m_EditorState.timelineZoom = std::max(0.1f, m_EditorState.timelineZoom - 0.2f);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS_PLUS)) {
        m_EditorState.timelineZoom = std::min(5.0f, m_EditorState.timelineZoom + 0.2f);
    }

    // Save indicator
    if (m_HasUnsavedChanges) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), ICON_FA_CIRCLE " Unsaved");
    }

    ImGui::PopStyleVar();
}

void SpriteAnimationEditorWindow::DrawClipSelector() {
    if (m_EditBuffer.clips.empty()) {
        if (ImGui::Button("Add First Clip")) {
            AddNewClip();
        }
        return;
    }

    // Clip dropdown
    std::vector<const char*> clipNames;
    for (const auto& clip : m_EditBuffer.clips) {
        clipNames.push_back(clip.name.c_str());
    }

    int currentClip = m_EditorState.selectedClipIndex;
    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo("##ClipSelector", &currentClip, clipNames.data(), (int)clipNames.size())) {
        m_EditorState.selectedClipIndex = currentClip;
        m_EditorState.selectedFrameIndex = -1;
        m_EditorState.currentTime = 0.0f;
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS)) {
        AddNewClip();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add new animation clip");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_CLONE)) {
        if (m_EditorState.selectedClipIndex >= 0) {
            DuplicateClip(m_EditorState.selectedClipIndex);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Duplicate current clip");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH)) {
        if (m_EditorState.selectedClipIndex >= 0) {
            DeleteClip(m_EditorState.selectedClipIndex);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Delete current clip");
    }
}

void SpriteAnimationEditorWindow::DrawTimeline() {
    if (m_EditorState.selectedClipIndex < 0 || m_EditBuffer.clips.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No clip selected");
        return;
    }

    auto& clip = m_EditBuffer.clips[m_EditorState.selectedClipIndex];

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    // Background
    ImVec4 bgColor = GetColorTimelineBg();
    drawList->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                           ImColor(bgColor.x, bgColor.y, bgColor.z, bgColor.w));

    // Draw ruler
    DrawTimelineRuler(canvas_size.x, 30.0f);

    // Draw frames track
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x, canvas_pos.y + 35.0f));
    DrawTimelineFrames(clip, canvas_size.x, canvas_size.y - 35.0f);

    // Draw playhead cursor
    DrawTimelineCursor(canvas_size.x, canvas_size.y);

    // Handle timeline input
    HandleTimelineInput();
}

void SpriteAnimationEditorWindow::DrawTimelineRuler(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // Calculate time range visible
    float pixelsPerSecond = 100.0f * m_EditorState.timelineZoom;
    float visibleTime = width / pixelsPerSecond;
    float startTime = m_EditorState.timelineScroll;

    // Draw time markers
    float timeStep = 0.1f; // 100ms intervals
    if (m_EditorState.timelineZoom < 0.5f) timeStep = 0.5f;
    else if (m_EditorState.timelineZoom > 2.0f) timeStep = 0.05f;

    for (float t = 0; t < visibleTime; t += timeStep) {
        float x = pos.x + (t * pixelsPerSecond);

        // Major tick every second
        bool isMajor = (int)(t * 10) % 10 == 0;
        float tickHeight = isMajor ? 15.0f : 8.0f;

        drawList->AddLine(ImVec2(x, pos.y + height - tickHeight),
                         ImVec2(x, pos.y + height),
                         ImColor(0.6f, 0.6f, 0.6f));

        // Draw time label for major ticks
        if (isMajor) {
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "%.1fs", startTime + t);
            drawList->AddText(ImVec2(x - 15, pos.y + 5), ImColor(0.8f, 0.8f, 0.8f), timeStr);
        }
    }

    // Bottom line
    drawList->AddLine(ImVec2(pos.x, pos.y + height),
                      ImVec2(pos.x + width, pos.y + height),
                      ImColor(0.4f, 0.4f, 0.4f));
}

void SpriteAnimationEditorWindow::DrawTimelineFrames(const SpriteAnimationClip& clip, float width, float height) {
    // Commented out to fix warning C4189 - unused variable
    // ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    float pixelsPerSecond = 100.0f * m_EditorState.timelineZoom;
    float currentX = 0.0f;
    float frameY = pos.y + 10.0f;
    float frameHeight = TRACK_HEIGHT;

    for (int i = 0; i < (int)clip.frames.size(); i++) {
        const auto& frame = clip.frames[i];
        float frameWidth = frame.duration * pixelsPerSecond;

        DrawFrameBlock(i, currentX / pixelsPerSecond, frame.duration,
                      frameY, frameHeight, i == m_EditorState.selectedFrameIndex);

        currentX += frameWidth;
    }

    // Make area interactive for frame selection
    ImGui::InvisibleButton("TimelineFrames", ImVec2(width, height));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
        // Calculate which frame was clicked
        ImVec2 mousePos = ImGui::GetMousePos();
        float relativeX = mousePos.x - pos.x;
        float clickTime = (relativeX / pixelsPerSecond) + m_EditorState.timelineScroll;

        int clickedFrame = GetFrameAtTime(clip, clickTime);
        if (clickedFrame >= 0) {
            m_EditorState.selectedFrameIndex = clickedFrame;
            m_EditorState.currentTime = GetFrameStartTime(clip, clickedFrame);
        }
    }
}

void SpriteAnimationEditorWindow::DrawFrameBlock(int frameIndex, float startTime, float duration,
                                                 float y, float height, bool selected) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    float pixelsPerSecond = 100.0f * m_EditorState.timelineZoom;
    float x = pos.x + (startTime - m_EditorState.timelineScroll) * pixelsPerSecond;
    float width = duration * pixelsPerSecond;

    // Frame background
    ImVec4 color = selected ? GetColorFrameSelected() : GetColorFrameNormal();
    if (!selected && ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + width, y + height))) {
        color = GetColorFrameHover();
    }

    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), ImColor(color));
    drawList->AddRect(ImVec2(x, y), ImVec2(x + width, y + height),
                     ImColor(0.2f, 0.2f, 0.2f), 2.0f);

    // Frame number
    if (width > 20) {
        char frameStr[16];
        snprintf(frameStr, sizeof(frameStr), "%d", frameIndex + 1);
        ImVec2 textSize = ImGui::CalcTextSize(frameStr);
        drawList->AddText(ImVec2(x + (width - textSize.x) / 2, y + (height - textSize.y) / 2),
                         ImColor(1.0f, 1.0f, 1.0f), frameStr);
    }
}

void SpriteAnimationEditorWindow::DrawTimelineCursor(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    float pixelsPerSecond = 100.0f * m_EditorState.timelineZoom;
    float cursorX = pos.x + (m_EditorState.currentTime - m_EditorState.timelineScroll) * pixelsPerSecond;

    // Draw vertical line
    ImVec4 cursorColor = GetColorTimelineCursor();
    drawList->AddLine(ImVec2(cursorX, pos.y), ImVec2(cursorX, pos.y + height),
                      ImColor(cursorColor.x, cursorColor.y, cursorColor.z, cursorColor.w), 2.0f);

    // Draw playhead triangle
    float triSize = 8.0f;
    drawList->AddTriangleFilled(
        ImVec2(cursorX - triSize, pos.y),
        ImVec2(cursorX + triSize, pos.y),
        ImVec2(cursorX, pos.y + triSize),
        ImColor(cursorColor.x, cursorColor.y, cursorColor.z, cursorColor.w));
}

void SpriteAnimationEditorWindow::DrawPreviewPanel() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImVec2 canvas_center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f,
                                  canvas_pos.y + canvas_size.y * 0.5f);

    // Background
    drawList->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                           ImColor(0.1f, 0.1f, 0.1f, 1.0f));

    // Preview controls in corner
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + 10, canvas_pos.y + 10));
    ImGui::BeginGroup();

    ImGui::Text("Preview Zoom: %.1fx", m_EditorState.previewZoom);
    ImGui::SameLine();
    if (ImGui::SmallButton("-")) {
        m_EditorState.previewZoom = std::max(0.1f, m_EditorState.previewZoom - 0.2f);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+")) {
        m_EditorState.previewZoom = std::min(10.0f, m_EditorState.previewZoom + 0.2f);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit")) {
        m_EditorState.previewZoom = 1.0f;
        m_EditorState.previewPan = glm::vec2(0.0f);
    }

    if (ImGui::Checkbox("Grid", &m_EditorState.showGrid)) {}
    ImGui::SameLine();
    if (ImGui::Checkbox("Bounds", &m_EditorState.showFrameBounds)) {}
    // Removed onion skin - not needed

    ImGui::EndGroup();

    // Draw grid
    if (m_EditorState.showGrid) {
        DrawPreviewGrid();
    }

    // Draw onion skin
    if (m_EditorState.enableOnionSkin) {
        DrawOnionSkin();
    }

    // Draw current sprite
    DrawPreviewSprite();

    // Handle preview input (pan/zoom)
    HandlePreviewInput();
}

void SpriteAnimationEditorWindow::DrawPreviewSprite() {
    if (m_EditorState.selectedClipIndex < 0 || m_EditBuffer.clips.empty()) return;

    auto& clip = m_EditBuffer.clips[m_EditorState.selectedClipIndex];
    if (clip.frames.empty()) return;

    int frameIndex = GetFrameAtTime(clip, m_EditorState.currentTime);
    if (frameIndex < 0) return;

    const auto& frame = clip.frames[frameIndex];

    // Check for empty GUID before trying to load texture
    if (frame.textureGUID == GUID_128{}) return;

    // Load texture
    std::string texturePath;
    try {
        texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(frame.textureGUID);
    } catch (...) {
        // Handle case where GUID is invalid or not found
        return;
    }

    if (texturePath.empty()) return;

    auto texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(frame.textureGUID, texturePath);
    if (!texture) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImVec2 canvas_center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f,
                                  canvas_pos.y + canvas_size.y * 0.5f);

    // Calculate sprite position and size
    // Since texture dimensions are not directly available in the Texture class,
    // use a default preview size. This could be improved by storing dimensions when loading.
    float spriteWidth = 256.0f * frame.uvScale.x;  // Default preview width
    float spriteHeight = 256.0f * frame.uvScale.y; // Default preview height

    ImVec2 spritePos = ImVec2(
        canvas_center.x + m_EditorState.previewPan.x * m_EditorState.previewZoom,
        canvas_center.y + m_EditorState.previewPan.y * m_EditorState.previewZoom
    );

    ImVec2 spriteSize = ImVec2(
        spriteWidth * m_EditorState.previewZoom,
        spriteHeight * m_EditorState.previewZoom
    );

    ImVec2 p0 = ImVec2(spritePos.x - spriteSize.x * 0.5f, spritePos.y - spriteSize.y * 0.5f);
    ImVec2 p1 = ImVec2(spritePos.x + spriteSize.x * 0.5f, spritePos.y + spriteSize.y * 0.5f);

    // UV coordinates
    ImVec2 uv0 = ImVec2(frame.uvOffset.x, frame.uvOffset.y);
    ImVec2 uv1 = ImVec2(frame.uvOffset.x + frame.uvScale.x, frame.uvOffset.y + frame.uvScale.y);

    // Draw sprite
    drawList->AddImage((ImTextureID)(intptr_t)texture->ID, p0, p1, uv0, uv1);

    // Draw bounds
    if (m_EditorState.showFrameBounds) {
        drawList->AddRect(p0, p1, ImColor(0.0f, 1.0f, 0.0f, 0.5f), 0.0f, 0, 2.0f);
    }
}

void SpriteAnimationEditorWindow::DrawPreviewGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    float gridSize = m_EditorState.gridSize * m_EditorState.previewZoom;

    ImVec4 gridColor = GetColorGrid();
    ImU32 gridColorU32 = ImColor(gridColor.x, gridColor.y, gridColor.z, gridColor.w);

    // Vertical lines
    for (float x = fmod(m_EditorState.previewPan.x * m_EditorState.previewZoom, gridSize);
         x < canvas_size.x; x += gridSize) {
        drawList->AddLine(
            ImVec2(canvas_pos.x + x, canvas_pos.y),
            ImVec2(canvas_pos.x + x, canvas_pos.y + canvas_size.y),
            gridColorU32);
    }

    // Horizontal lines
    for (float y = fmod(m_EditorState.previewPan.y * m_EditorState.previewZoom, gridSize);
         y < canvas_size.y; y += gridSize) {
        drawList->AddLine(
            ImVec2(canvas_pos.x, canvas_pos.y + y),
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y),
            gridColorU32);
    }

    // Center axes
    ImVec2 center = ImVec2(
        canvas_pos.x + canvas_size.x * 0.5f + m_EditorState.previewPan.x * m_EditorState.previewZoom,
        canvas_pos.y + canvas_size.y * 0.5f + m_EditorState.previewPan.y * m_EditorState.previewZoom
    );

    drawList->AddLine(
        ImVec2(center.x, canvas_pos.y),
        ImVec2(center.x, canvas_pos.y + canvas_size.y),
        ImColor(1.0f, 0.0f, 0.0f, 0.5f), 2.0f);

    drawList->AddLine(
        ImVec2(canvas_pos.x, center.y),
        ImVec2(canvas_pos.x + canvas_size.x, center.y),
        ImColor(0.0f, 1.0f, 0.0f, 0.5f), 2.0f);
}

void SpriteAnimationEditorWindow::DrawOnionSkin() {
    if (m_EditorState.selectedClipIndex < 0 || m_EditBuffer.clips.empty()) return;

    auto& clip = m_EditBuffer.clips[m_EditorState.selectedClipIndex];
    if (clip.frames.empty()) return;

    int currentFrame = GetFrameAtTime(clip, m_EditorState.currentTime);
    if (currentFrame < 0) return;

    // Commented out to fix warning C4189 - unused variable
    // ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw previous frames
    for (int i = 1; i <= m_EditorState.onionSkinBefore; i++) {
        int frameIndex = currentFrame - i;
        if (frameIndex >= 0) {
            // Commented out to fix warning C4189 - unused variable
            // Draw with decreasing alpha
            // float alpha = m_EditorState.onionSkinAlpha * (1.0f - (float)i / (m_EditorState.onionSkinBefore + 1));
            // Similar drawing code as DrawPreviewSprite but with alpha
        }
    }

    // Draw future frames
    for (int i = 1; i <= m_EditorState.onionSkinAfter; i++) {
        int frameIndex = currentFrame + i;
        if (frameIndex < (int)clip.frames.size()) {
            // Commented out to fix warning C4189 - unused variable
            // Draw with decreasing alpha
            // float alpha = m_EditorState.onionSkinAlpha * (1.0f - (float)i / (m_EditorState.onionSkinAfter + 1));
            // Similar drawing code as DrawPreviewSprite but with alpha
        }
    }
}

void SpriteAnimationEditorWindow::DrawPropertiesPanel() {
    if (ImGui::CollapsingHeader("Clip Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_EditorState.selectedClipIndex >= 0 && m_EditorState.selectedClipIndex < (int)m_EditBuffer.clips.size()) {
            auto& clip = m_EditBuffer.clips[m_EditorState.selectedClipIndex];

            char nameBuf[256] = {};
            clip.name.copy(nameBuf, sizeof(nameBuf) - 1);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                clip.name = nameBuf;
                m_HasUnsavedChanges = true;
            }

            if (ImGui::Checkbox("Loop", &clip.loop)) {
                m_HasUnsavedChanges = true;
            }

            float totalDuration = GetTotalClipDuration(clip);
            ImGui::Text("Duration: %.2fs", totalDuration);
            ImGui::Text("Frame Count: %d", (int)clip.frames.size());
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Frame Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_EditorState.selectedClipIndex >= 0 && m_EditorState.selectedFrameIndex >= 0) {
            auto& clip = m_EditBuffer.clips[m_EditorState.selectedClipIndex];
            if (m_EditorState.selectedFrameIndex < (int)clip.frames.size()) {
                auto& frame = clip.frames[m_EditorState.selectedFrameIndex];

                ImGui::Text("Frame %d", m_EditorState.selectedFrameIndex + 1);

                // Texture field with drag-drop support
                std::string texDisplay = "None";
                if (!frame.texturePath.empty()) {
                    // Show only filename, not full path
                    size_t lastSlash = frame.texturePath.find_last_of("/\\");
                    texDisplay = (lastSlash != std::string::npos) ?
                                frame.texturePath.substr(lastSlash + 1) : frame.texturePath;
                }

                // Create a button that looks like a text field for drag-drop
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));

                ImGui::Button(("Texture: " + texDisplay + "###TextureField").c_str(),
                             ImVec2(ImGui::GetContentRegionAvail().x - 80, 0));

                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                if (ImGui::Button("Browse")) {
                    // Open file browser
                }

                // Drag-drop target for the texture field
                if (EditorComponents::BeginDragDropTarget()) {
                    ImGui::SetTooltip("Drop texture here");
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD")) {
                        const char* texturePath = (const char*)payload->Data;
                        std::string pathStr(texturePath, payload->DataSize);
                        pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                        frame.texturePath = pathStr;
                        frame.textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(pathStr);
                        m_HasUnsavedChanges = true;
                    }
                    EditorComponents::EndDragDropTarget();
                }

                // Duration
                if (ImGui::DragFloat("Duration", &frame.duration, 0.01f, 0.01f, 10.0f, "%.3fs")) {
                    m_HasUnsavedChanges = true;
                }

                // UV coordinates
                ImGui::Separator();
                ImGui::Text("UV Coordinates");

                float uvOffset[2] = {frame.uvOffset.x, frame.uvOffset.y};
                if (ImGui::DragFloat2("UV Offset", uvOffset, 0.01f, 0.0f, 1.0f)) {
                    frame.uvOffset.x = uvOffset[0];
                    frame.uvOffset.y = uvOffset[1];
                    m_HasUnsavedChanges = true;
                }

                float uvScale[2] = {frame.uvScale.x, frame.uvScale.y};
                if (ImGui::DragFloat2("UV Scale", uvScale, 0.01f, 0.0f, 1.0f)) {
                    frame.uvScale.x = uvScale[0];
                    frame.uvScale.y = uvScale[1];
                    m_HasUnsavedChanges = true;
                }

                if (ImGui::Button("Open Sprite Sheet Editor")) {
                    m_EditorState.spriteSheetMode = true;
                    m_EditorState.currentTextureGUID = frame.textureGUID;
                }
            }
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Frame List")) {
        if (m_EditorState.selectedClipIndex >= 0) {
            auto& clip = m_EditBuffer.clips[m_EditorState.selectedClipIndex];

            for (int i = 0; i < (int)clip.frames.size(); i++) {
                ImGui::PushID(i);

                // Frame selectable with delete button
                bool isSelected = (i == m_EditorState.selectedFrameIndex);

                // Calculate width for selectable
                float availWidth = ImGui::GetContentRegionAvail().x;
                if (ImGui::Selectable(("Frame " + std::to_string(i + 1)).c_str(), isSelected,
                                     ImGuiSelectableFlags_None, ImVec2(availWidth - 30, 0))) {
                    m_EditorState.selectedFrameIndex = i;
                    m_EditorState.currentTime = GetFrameStartTime(clip, i);
                }

                // Right-click context menu for delete
                if (ImGui::BeginPopupContextItem(("frame_context_" + std::to_string(i)).c_str())) {
                    if (ImGui::MenuItem("Delete Frame")) {
                        DeleteFrame(m_EditorState.selectedClipIndex, i);
                        ImGui::EndPopup();
                        ImGui::PopID();
                        break; // Exit loop as frames array has changed
                    }
                    ImGui::EndPopup();
                }

                // X button for delete
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.3f));
                if (ImGui::Button(("X##del" + std::to_string(i)).c_str(), ImVec2(20, 0))) {
                    DeleteFrame(m_EditorState.selectedClipIndex, i);
                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                    break; // Exit loop as frames array has changed
                }
                ImGui::PopStyleColor(2);

                // Drag to reorder
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("FRAME_REORDER", &i, sizeof(int));
                    ImGui::Text("Frame %d", i + 1);
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FRAME_REORDER")) {
                        int sourceIndex = *(const int*)payload->Data;
                        if (sourceIndex != i) {
                            MoveFrame(m_EditorState.selectedClipIndex, sourceIndex, i);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::PopID();
            }

            if (ImGui::Button(ICON_FA_PLUS " Add Frame")) {
                AddNewFrame(m_EditorState.selectedClipIndex);
            }
        }
    }
}

void SpriteAnimationEditorWindow::UpdatePreviewAnimation(float deltaTime) {
    if (m_EditorState.selectedClipIndex < 0 || m_EditBuffer.clips.empty()) return;

    auto& clip = m_EditBuffer.clips[m_EditorState.selectedClipIndex];
    if (clip.frames.empty()) return;

    m_EditorState.currentTime += deltaTime * m_EditorState.playbackSpeed;

    float totalDuration = GetTotalClipDuration(clip);
    if (m_EditorState.currentTime >= totalDuration) {
        if (clip.loop) {
            m_EditorState.currentTime = fmod(m_EditorState.currentTime, totalDuration);
        } else {
            m_EditorState.currentTime = totalDuration;
            m_EditorState.isPlaying = false;
        }
    }
}

void SpriteAnimationEditorWindow::HandleKeyboardShortcuts() {
    if (!ImGui::IsWindowFocused()) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        m_EditorState.isPlaying = !m_EditorState.isPlaying;
    }

    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        SaveAnimation();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (m_EditorState.selectedFrameIndex >= 0) {
            DeleteFrame(m_EditorState.selectedClipIndex, m_EditorState.selectedFrameIndex);
        }
    }

    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        if (m_EditorState.selectedFrameIndex >= 0) {
            DuplicateFrame(m_EditorState.selectedClipIndex, m_EditorState.selectedFrameIndex);
        }
    }
}

void SpriteAnimationEditorWindow::HandleTimelineInput() {
    // Implement timeline scrolling and interaction
    if (ImGui::IsWindowHovered()) {
        if (ImGui::GetIO().MouseWheel != 0) {
            m_EditorState.timelineZoom += ImGui::GetIO().MouseWheel * 0.1f;
            m_EditorState.timelineZoom = std::max(0.1f, std::min(5.0f, m_EditorState.timelineZoom));
        }
    }
}

void SpriteAnimationEditorWindow::HandlePreviewInput() {
    if (!ImGui::IsWindowHovered()) return;

    // Pan with middle mouse
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_EditorState.previewPan.x += delta.x / m_EditorState.previewZoom;
        m_EditorState.previewPan.y += delta.y / m_EditorState.previewZoom;
    }

    // Zoom with scroll wheel
    if (ImGui::GetIO().MouseWheel != 0) {
        float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
        m_EditorState.previewZoom = std::max(0.1f, std::min(10.0f, m_EditorState.previewZoom + zoomDelta));
    }
}

// Frame operations
void SpriteAnimationEditorWindow::AddNewFrame(int clipIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_EditBuffer.clips.size()) return;

    SpriteFrame newFrame;
    newFrame.duration = 0.1f;
    newFrame.uvOffset = glm::vec2(0.0f);
    newFrame.uvScale = glm::vec2(1.0f);

    m_EditBuffer.clips[clipIndex].frames.push_back(newFrame);
    m_EditorState.selectedFrameIndex = (int)m_EditBuffer.clips[clipIndex].frames.size() - 1;
    m_HasUnsavedChanges = true;

    // Apply changes to component without taking snapshot
    if (m_AnimComponent) {
        m_AnimComponent->clips[clipIndex] = m_EditBuffer.clips[clipIndex];
        UpdateSpriteRenderComponent();
    }
}

void SpriteAnimationEditorWindow::DeleteFrame(int clipIndex, int frameIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_EditBuffer.clips.size()) return;

    auto& clip = m_EditBuffer.clips[clipIndex];
    if (frameIndex < 0 || frameIndex >= (int)clip.frames.size()) return;

    clip.frames.erase(clip.frames.begin() + frameIndex);

    if (m_EditorState.selectedFrameIndex >= (int)clip.frames.size()) {
        m_EditorState.selectedFrameIndex = (int)clip.frames.size() - 1;
    }

    m_HasUnsavedChanges = true;

    // Apply changes to component without taking snapshot
    if (m_AnimComponent) {
        m_AnimComponent->clips[clipIndex] = clip;
    }
}

void SpriteAnimationEditorWindow::DuplicateFrame(int clipIndex, int frameIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_EditBuffer.clips.size()) return;

    auto& clip = m_EditBuffer.clips[clipIndex];
    if (frameIndex < 0 || frameIndex >= (int)clip.frames.size()) return;

    SpriteFrame duplicated = clip.frames[frameIndex];
    clip.frames.insert(clip.frames.begin() + frameIndex + 1, duplicated);
    m_EditorState.selectedFrameIndex = frameIndex + 1;
    m_HasUnsavedChanges = true;
}

void SpriteAnimationEditorWindow::MoveFrame(int clipIndex, int fromIndex, int toIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_EditBuffer.clips.size()) return;

    auto& clip = m_EditBuffer.clips[clipIndex];
    if (fromIndex < 0 || fromIndex >= (int)clip.frames.size()) return;
    if (toIndex < 0 || toIndex >= (int)clip.frames.size()) return;

    if (fromIndex == toIndex) return;

    SpriteFrame temp = clip.frames[fromIndex];
    clip.frames.erase(clip.frames.begin() + fromIndex);

    if (toIndex > fromIndex) toIndex--;
    clip.frames.insert(clip.frames.begin() + toIndex, temp);

    m_EditorState.selectedFrameIndex = toIndex;
    m_HasUnsavedChanges = true;
}

// Clip operations
void SpriteAnimationEditorWindow::AddNewClip() {
    SpriteAnimationClip newClip;
    newClip.name = "New Clip " + std::to_string(m_EditBuffer.clips.size() + 1);
    newClip.loop = true;

    m_EditBuffer.clips.push_back(newClip);
    m_EditorState.selectedClipIndex = (int)m_EditBuffer.clips.size() - 1;
    m_EditorState.selectedFrameIndex = -1;
    m_HasUnsavedChanges = true;

    // Apply changes to component without taking snapshot
    if (m_AnimComponent) {
        m_AnimComponent->clips = m_EditBuffer.clips;
        UpdateSpriteRenderComponent();
    }
}

void SpriteAnimationEditorWindow::DeleteClip(int clipIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_EditBuffer.clips.size()) return;

    m_EditBuffer.clips.erase(m_EditBuffer.clips.begin() + clipIndex);

    // Adjust selection
    if (m_EditorState.selectedClipIndex >= (int)m_EditBuffer.clips.size()) {
        m_EditorState.selectedClipIndex = (int)m_EditBuffer.clips.size() - 1;
    }

    // If we deleted the current clip, update currentClipIndex in the component
    if (clipIndex == m_EditBuffer.currentClipIndex) {
        m_EditBuffer.currentClipIndex = m_EditorState.selectedClipIndex;
    } else if (clipIndex < m_EditBuffer.currentClipIndex) {
        // Adjust currentClipIndex if we deleted a clip before it
        m_EditBuffer.currentClipIndex--;
    }

    m_EditorState.selectedFrameIndex = -1;
    m_HasUnsavedChanges = true;

    // Apply changes to component without taking snapshot
    if (m_AnimComponent) {
        m_AnimComponent->clips = m_EditBuffer.clips;
        m_AnimComponent->currentClipIndex = m_EditBuffer.currentClipIndex;
        UpdateSpriteRenderComponent();
    }
}

void SpriteAnimationEditorWindow::DuplicateClip(int clipIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_EditBuffer.clips.size()) return;

    SpriteAnimationClip duplicated = m_EditBuffer.clips[clipIndex];
    duplicated.name += " (Copy)";

    m_EditBuffer.clips.push_back(duplicated);
    m_EditorState.selectedClipIndex = (int)m_EditBuffer.clips.size() - 1;
    m_HasUnsavedChanges = true;
}

void SpriteAnimationEditorWindow::SaveAnimation() {
    if (!m_AnimComponent) return;

    // Copy edited data back to the actual component
    *m_AnimComponent = m_EditBuffer;
    m_HasUnsavedChanges = false;

    // Update the sprite render component with the first frame if we have one
    UpdateSpriteRenderComponent();

    // Take snapshot for undo
    SnapshotManager::GetInstance().TakeSnapshot("Edit Animation");
}

void SpriteAnimationEditorWindow::UpdateSpriteRenderComponent() {
    if (m_CurrentEntity == 0 || !m_AnimComponent) return;

    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    if (ecs.HasComponent<SpriteRenderComponent>(m_CurrentEntity)) {
        auto& sprite = ecs.GetComponent<SpriteRenderComponent>(m_CurrentEntity);

        // Set the sprite to show the current frame
        if (m_AnimComponent->currentClipIndex >= 0 &&
            m_AnimComponent->currentClipIndex < (int)m_AnimComponent->clips.size()) {
            const auto& clip = m_AnimComponent->clips[m_AnimComponent->currentClipIndex];
            if (!clip.frames.empty()) {
                int frameIdx = m_AnimComponent->currentFrameIndex >= 0 &&
                               m_AnimComponent->currentFrameIndex < (int)clip.frames.size() ?
                               m_AnimComponent->currentFrameIndex : 0;
                const auto& frame = clip.frames[frameIdx];

                // Update sprite with frame data
                sprite.textureGUID = frame.textureGUID;
                sprite.texturePath = frame.texturePath;
                sprite.uvOffset = frame.uvOffset;
                sprite.uvScale = frame.uvScale;

                // Load the texture resource if needed
                if (frame.textureGUID != GUID_128{}) {
                    std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(frame.textureGUID);
                    sprite.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(frame.textureGUID, texturePath);
                }
            }
        }
    }
}

// Utility functions
float SpriteAnimationEditorWindow::GetTotalClipDuration(const SpriteAnimationClip& clip) const {
    float total = 0.0f;
    for (const auto& frame : clip.frames) {
        total += frame.duration;
    }
    return total;
}

int SpriteAnimationEditorWindow::GetFrameAtTime(const SpriteAnimationClip& clip, float time) const {
    if (clip.frames.empty()) return -1;

    float currentTime = 0.0f;
    for (int i = 0; i < (int)clip.frames.size(); i++) {
        currentTime += clip.frames[i].duration;
        if (time < currentTime) {
            return i;
        }
    }
    return (int)clip.frames.size() - 1;
}

float SpriteAnimationEditorWindow::GetFrameStartTime(const SpriteAnimationClip& clip, int frameIndex) const {
    if (frameIndex < 0 || frameIndex >= (int)clip.frames.size()) return 0.0f;

    float time = 0.0f;
    for (int i = 0; i < frameIndex; i++) {
        time += clip.frames[i].duration;
    }
    return time;
}