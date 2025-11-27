#include "pch.h"
#include "ReflectionRenderer.hpp"
#include "Graphics/Sprite/SpriteAnimationComponent.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Panels/SpriteAnimationEditorWindow.hpp"
#include "ECS/ECSManager.hpp"
#include "EditorComponents.hpp"
#include "EditorState.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Graphics/Texture.h"
#include "SnapshotManager.hpp"
#include "imgui.h"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <algorithm>

void RegisterSpriteAnimationInspector() {
    // ==================== SPRITE ANIMATION COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("SpriteAnimationComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        SpriteAnimationComponent &anim = *static_cast<SpriteAnimationComponent *>(componentPtr);
        // Commented out to fix warning C4189 - unused variable
        // const float labelWidth = EditorComponents::GetLabelWidth();

        // Get the animation editor window
        auto* animEditor = GetSpriteAnimationEditor();

        // Check if this entity is being edited in the animation editor
        bool isBeingEdited = animEditor->IsEditingEntity(entity);

        // Main editor button
        ImGui::PushStyleColor(ImGuiCol_Button, isBeingEdited ?
            ImVec4(0.3f, 0.6f, 0.9f, 1.0f) : ImVec4(0.2f, 0.4f, 0.6f, 1.0f));

        std::string buttonText = isBeingEdited ?
            ICON_FA_FILM " Animation Editor (Open)" : ICON_FA_FILM " Open Animation Editor";

        if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 40))) {
            if (isBeingEdited) {
                // Bring window to front
                ImGui::SetWindowFocus("Sprite Animation Editor");
            } else {
                // Open the animation editor for this entity
                animEditor->OpenForEntity(entity, &anim);
            }
        }

        ImGui::PopStyleColor();

        // Show animation summary
        ImGui::Separator();
        ImGui::Text("Animation Summary:");

        // Clips info
        ImGui::Text("Clips: %d", (int)anim.clips.size());

        if (!anim.clips.empty()) {
            // Current clip info
            if (anim.currentClipIndex >= 0 && anim.currentClipIndex < (int)anim.clips.size()) {
                auto& currentClip = anim.clips[anim.currentClipIndex];
                ImGui::Text("Current: %s", currentClip.name.c_str());
                ImGui::Text("Frames: %d", (int)currentClip.frames.size());
                ImGui::Text("Loop: %s", currentClip.loop ? "Yes" : "No");

                // Calculate total duration
                float totalDuration = 0.0f;
                for (const auto& frame : currentClip.frames) {
                    totalDuration += frame.duration;
                }
                ImGui::Text("Duration: %.2fs", totalDuration);
            }

            // Quick clip selector
            ImGui::Separator();
            ImGui::Text("Quick Select:");

            for (int i = 0; i < (int)anim.clips.size(); i++) {
                ImGui::PushID(i);

                bool isSelected = (i == anim.currentClipIndex);
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
                }

                if (ImGui::SmallButton(anim.clips[i].name.c_str())) {
                    anim.currentClipIndex = i;
                    anim.currentFrameIndex = 0;
                    anim.timeInCurrentFrame = 0.0f;
                    SnapshotManager::GetInstance().TakeSnapshot("Select Animation Clip");
                }

                if (isSelected) {
                    ImGui::PopStyleColor();
                }

                if ((i + 1) % 3 != 0 && i < (int)anim.clips.size() - 1) {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No animation clips");
            ImGui::Text("Open the editor to create clips");
        }

        // Quick preview controls (only in edit mode)
        if (EditorState::GetInstance().GetState() == EditorState::State::EDIT_MODE) {
            // Preview state management
            enum class PreviewState {
                Stopped,
                Playing,
                Paused
            };
            static std::unordered_map<Entity, PreviewState> previewStates;

            if (previewStates.find(entity) == previewStates.end()) {
                previewStates[entity] = PreviewState::Stopped;
            }

            // Handle preview animation updates
            if (previewStates[entity] == PreviewState::Playing) {
                if (anim.currentClipIndex >= 0 && anim.currentClipIndex < static_cast<int>(anim.clips.size())) {
                    SpriteAnimationClip& clip = anim.clips[anim.currentClipIndex];
                    if (!clip.frames.empty()) {
                        // Update preview time
                        float deltaTime = ImGui::GetIO().DeltaTime;
                        anim.editorPreviewTime += deltaTime * anim.playbackSpeed;

                        // Check if we need to advance frame
                        while (anim.editorPreviewFrameIndex < (int)clip.frames.size() &&
                               anim.editorPreviewTime >= clip.frames[anim.editorPreviewFrameIndex].duration) {
                            anim.editorPreviewTime -= clip.frames[anim.editorPreviewFrameIndex].duration;
                            anim.editorPreviewFrameIndex++;

                            if (anim.editorPreviewFrameIndex >= static_cast<int>(clip.frames.size())) {
                                if (clip.loop) {
                                    anim.editorPreviewFrameIndex = 0;
                                } else {
                                    anim.editorPreviewFrameIndex = static_cast<int>(clip.frames.size()) - 1;
                                    previewStates[entity] = PreviewState::Paused;
                                    break;
                                }
                            }
                        }

                        // Update the sprite renderer with current frame data
                        if (ecs.HasComponent<SpriteRenderComponent>(entity)) {
                            auto& sprite = ecs.GetComponent<SpriteRenderComponent>(entity);
                            const SpriteFrame& frame = clip.frames[anim.editorPreviewFrameIndex];

                            // Update texture if different
                            if (frame.textureGUID != GUID_128{} && frame.textureGUID != sprite.textureGUID) {
                                sprite.textureGUID = frame.textureGUID;
                                std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(frame.textureGUID);
                                sprite.texturePath = texturePath;
                                sprite.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(frame.textureGUID, texturePath);
                            }

                            // Update UV coordinates
                            sprite.uvOffset = frame.uvOffset;
                            sprite.uvScale = frame.uvScale;
                        }
                    }
                }
            }

            // Quick preview controls
            ImGui::Separator();
            ImGui::Text("Quick Preview:");

            if (previewStates[entity] == PreviewState::Playing) {
                if (ImGui::SmallButton(ICON_FA_PAUSE)) {
                    previewStates[entity] = PreviewState::Paused;
                }
            } else {
                if (ImGui::SmallButton(ICON_FA_PLAY)) {
                    previewStates[entity] = PreviewState::Playing;
                    if (previewStates[entity] == PreviewState::Stopped) {
                        anim.editorPreviewTime = 0.0f;
                        anim.editorPreviewFrameIndex = 0;
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_STOP)) {
                previewStates[entity] = PreviewState::Stopped;
                anim.editorPreviewTime = 0.0f;
                anim.editorPreviewFrameIndex = 0;

                // Reset sprite to first frame
                if (ecs.HasComponent<SpriteRenderComponent>(entity)) {
                    auto& sprite = ecs.GetComponent<SpriteRenderComponent>(entity);
                    if (anim.currentClipIndex >= 0 && anim.currentClipIndex < static_cast<int>(anim.clips.size())) {
                        SpriteAnimationClip& clip = anim.clips[anim.currentClipIndex];
                        if (!clip.frames.empty()) {
                            const SpriteFrame& frame = clip.frames[0];
                            if (frame.textureGUID != GUID_128{}) {
                                sprite.textureGUID = frame.textureGUID;
                                std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(frame.textureGUID);
                                sprite.texturePath = texturePath;
                                sprite.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(frame.textureGUID, texturePath);
                            }
                            sprite.uvOffset = frame.uvOffset;
                            sprite.uvScale = frame.uvScale;
                        }
                    }
                }
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::DragFloat("##Speed", &anim.playbackSpeed, 0.01f, 0.1f, 5.0f, "%.1fx");

            // Preview status
            const char* statusText = "Stopped";
            ImVec4 statusColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

            if (previewStates[entity] == PreviewState::Playing) {
                statusText = "Playing";
                statusColor = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
            } else if (previewStates[entity] == PreviewState::Paused) {
                statusText = "Paused";
                statusColor = ImVec4(0.8f, 0.8f, 0.3f, 1.0f);
            }

            ImGui::SameLine();
            ImGui::TextColored(statusColor, "[%s]", statusText);

            // Current frame info
            if (anim.currentClipIndex >= 0 && anim.currentClipIndex < (int)anim.clips.size()) {
                auto& clip = anim.clips[anim.currentClipIndex];
                if (!clip.frames.empty()) {
                    int frameIndex = previewStates[entity] != PreviewState::Stopped ?
                        anim.editorPreviewFrameIndex : anim.currentFrameIndex;
                    ImGui::Text("Frame: %d/%d", frameIndex + 1, (int)clip.frames.size());
                }
            }
        }

        return true; // Skip default rendering
    });
}