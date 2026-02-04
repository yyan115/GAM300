#pragma once

#include "pch.h"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Video/VideoSystem.hpp"
#include "Video/VideoComponent.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Input/InputManager.h"
#include "ECS/TagManager.hpp"
#include "ECS/TagComponent.hpp"
#include "Sound/AudioComponent.hpp"
#include "Video/cutscenelayer.hpp"
#include "Scene/SceneManager.hpp"
#include <algorithm>
#include <cmath>

//HELPER FUNCTIONS
float lerp(float start, float end, float time)
{
    return start + time * (end - start);
}

// Smooth ease-in-out for cinematic transitions
float smoothstep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Ease-out cubic for smoother deceleration
float easeOutCubic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

// Panel definitions: which frames belong to which panel
// Panel 1: frames 0-2, Panel 2: frames 3-5, Panel 3: frames 6-8, Panel 4: frames 9-10, Panel 5: frames 11-12
int VideoSystem::GetPanelForFrame(int frame) const
{
    if (frame <= 2) return 1;
    if (frame <= 5) return 2;
    if (frame <= 8) return 3;
    if (frame <= 10) return 4;
    return 5;
}

int VideoSystem::GetFirstFrameOfPanel(int panel) const
{
    switch (panel) {
        case 1: return 0;
        case 2: return 3;
        case 3: return 6;
        case 4: return 9;
        case 5: return 11;
        default: return 0;
    }
}

int VideoSystem::GetLastFrameOfPanel(int panel) const
{
    switch (panel) {
        case 1: return 2;
        case 2: return 5;
        case 3: return 8;
        case 4: return 10;
        case 5: return 12;
        default: return 12;
    }
}

bool VideoSystem::IsLastFrameInPanel(int frame) const
{
    int panel = GetPanelForFrame(frame);
    return frame == GetLastFrameOfPanel(panel);
}

void VideoSystem::Initialise(ECSManager& ecsManager) {
     std::cout << "VideoSystem Initialised" << std::endl;
     m_ecs = &ecsManager;
}

void VideoSystem::Update(float dt) {
    PROFILE_FUNCTION();

    int dialogueTagIndex = TagManager::GetInstance().GetTagIndex("DialogueText");
    int dialogueBoxTagIndex = TagManager::GetInstance().GetTagIndex("DialogueBox");
    int blackScreenIndex = TagManager::GetInstance().GetTagIndex("BlackScreen");
    int skipButtonIndex = TagManager::GetInstance().GetTagIndex("SkipButton");

    // GET ENTITY VIA TAG
    for (const auto& entity : m_ecs->GetAllEntities())
    {
        if (foundBlackScreen && foundDialogueBox && foundDialogueText && foundSkipButton)
            break;

        // FIND DialogueText Entity
        if (!foundDialogueText &&
            m_ecs->HasComponent<TextRenderComponent>(entity) &&
            m_ecs->GetComponent<TagComponent>(entity).tagIndex == dialogueTagIndex)
        {
            foundDialogueText = true;
            dialogueText_Entity = entity;
        }

        // FIND DIALOGUEBOX ENTITY
        if (!foundDialogueBox &&
            m_ecs->HasComponent<TextRenderComponent>(entity) &&
            m_ecs->GetComponent<TagComponent>(entity).tagIndex == dialogueBoxTagIndex)
        {
            foundDialogueBox = true;
            dialogueBox_Entity = entity;
        }

        // FIND BLACKSCREEN ENTITY FOR TRANSITION
        if (!foundBlackScreen &&
            m_ecs->HasComponent<SpriteRenderComponent>(entity) &&
            m_ecs->GetComponent<TagComponent>(entity).tagIndex == blackScreenIndex)
        {
            foundBlackScreen = true;
            blackScreen_Entity = entity;
        }

        // FIND SKIP BUTTON ENTITY
        if (!foundSkipButton &&
            m_ecs->HasComponent<SpriteRenderComponent>(entity) &&
            m_ecs->GetComponent<TagComponent>(entity).tagIndex == skipButtonIndex)
        {
            foundSkipButton = true;
            skipButton_Entity = entity;
        }
    }

    if (m_ecs->HasComponent<TextRenderComponent>(dialogueText_Entity) == false)
        return;

    // GET RESPECTIVE COMPONENTS
    auto& textComp = m_ecs->GetComponent<TextRenderComponent>(dialogueText_Entity);
    auto& textTransform = m_ecs->GetComponent<Transform>(dialogueText_Entity);
    auto& textAudioComp = m_ecs->GetComponent<AudioComponent>(dialogueText_Entity);
    auto& blackScreenSprite = m_ecs->GetComponent<SpriteRenderComponent>(blackScreen_Entity);

    SpriteRenderComponent* skipButtonSprite = nullptr;
    if (foundSkipButton && m_ecs->HasComponent<SpriteRenderComponent>(skipButton_Entity))
        skipButtonSprite = &m_ecs->GetComponent<SpriteRenderComponent>(skipButton_Entity);

    // GET THE VIDEO COMP AND SPRITE COMPONENT FROM THE ENTITY
    for (const auto& entity : entities)
    {
        if (!m_ecs->HasComponent<VideoComponent>(entity) || !m_ecs->HasComponent<SpriteRenderComponent>(entity))
            continue;
        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!m_ecs->IsEntityActiveInHierarchy(entity)) continue;

        auto& videoComp = m_ecs->GetComponent<VideoComponent>(entity);
        auto& spriteComp = m_ecs->GetComponent<SpriteRenderComponent>(entity);

        // SET UP THE FIRST CUTSCENE
        if (videoComp.asset_dirty)
        {
            videoComp.activeFrame = videoComp.frameStart;
            videoComp.currentTime = 0;
            videoComp.currentPanel = GetPanelForFrame(videoComp.frameStart);
            videoComp.asset_dirty = false;
            m_boardTimer = 0.0f;
            m_fadeTimer = 0.0f;
            m_isFading = false;
            m_fadeTargetFrame = -1;
            isSkipping = false;

            std::string firstPath = ConstructNewPath(videoComp);
            m_dialogueManager.Reset();
            SwapCutscene(spriteComp, firstPath);
            isTransitioning = true;
            videoComp.cutsceneEnded = false;
            internalCutsceneEnded = false;
            blackScreenSprite.alpha = 1.0f;  // Start from black
        }

        videoComp.currentTime += dt;

        // ========== SKIP BUTTON HANDLING ==========
        // Check if cutsceneEnded was set externally (e.g., by Lua skip script)
        if (videoComp.cutsceneEnded && !internalCutsceneEnded && !isSkipping)
        {
            // Skip was triggered externally, start skip fade
            isSkipping = true;
            internalCutsceneEnded = true;
            m_isFading = true;
            m_fadeTimer = 0.0f;
            videoComp.cutsceneEnded = false;  // Reset so we can use proper fade
        }

        // ========== HANDLE FADE TRANSITIONS ==========
        if (m_isFading)
        {
            m_fadeTimer += dt;
            float fadeDuration = isSkipping ? videoComp.skipFadeDuration : videoComp.fadeDuration;
            float fadeProgress = std::clamp(m_fadeTimer / fadeDuration, 0.0f, 1.0f);
            float easedProgress = smoothstep(fadeProgress);

            if (internalCutsceneEnded || isSkipping)
            {
                // Fade OUT to black (ending cutscene)
                blackScreenSprite.alpha = easedProgress;

                if (fadeProgress >= 1.0f)
                {
                    blackScreenSprite.alpha = 1.0f;
                    m_isFading = false;
                    videoComp.cutsceneEnded = true;

                    // Hide skip button after skipping
                    if (skipButtonSprite)
                        skipButtonSprite->isVisible = false;
                }
            }
            else if (m_fadeTargetFrame >= 0)
            {
                // Fade between boards
                // First half: fade to black
                if (fadeProgress < 0.5f)
                {
                    blackScreenSprite.alpha = smoothstep(fadeProgress * 2.0f);
                }
                // At midpoint: swap the image
                else if (fadeProgress >= 0.5f && videoComp.activeFrame != m_fadeTargetFrame)
                {
                    videoComp.activeFrame = m_fadeTargetFrame;
                    videoComp.currentPanel = GetPanelForFrame(m_fadeTargetFrame);
                    std::string newPath = ConstructNewPath(videoComp);
                    SwapCutscene(spriteComp, newPath);

                    // Reset dialogue for new panel
                    int newPanel = GetPanelForFrame(m_fadeTargetFrame);
                    int oldPanel = GetPanelForFrame(m_fadeTargetFrame - 1);
                    if (newPanel != oldPanel)
                    {
                        m_dialogueManager.Reset();
                    }
                }
                // Second half: fade from black
                if (fadeProgress >= 0.5f)
                {
                    blackScreenSprite.alpha = 1.0f - smoothstep((fadeProgress - 0.5f) * 2.0f);
                }

                if (fadeProgress >= 1.0f)
                {
                    blackScreenSprite.alpha = 0.0f;
                    m_isFading = false;
                    m_fadeTargetFrame = -1;
                    m_boardTimer = 0.0f;  // Reset board timer after transition
                }
            }
            else
            {
                // Initial fade in from black
                blackScreenSprite.alpha = 1.0f - easedProgress;

                if (fadeProgress >= 1.0f)
                {
                    blackScreenSprite.alpha = 0.0f;
                    m_isFading = false;
                    isTransitioning = false;
                    m_boardTimer = 0.0f;
                }
            }

            // Continue to render text even during fade
            goto render_text;
        }

        // ========== INITIAL TRANSITION ==========
        if (isTransitioning && !m_isFading)
        {
            m_isFading = true;
            m_fadeTimer = 0.0f;
            m_fadeTargetFrame = -1;  // No target, just fade in
            goto render_text;
        }

        // ========== BLOCK INPUT IF CUTSCENE ENDED ==========
        if (internalCutsceneEnded)
        {
            if (!m_isFading)
            {
                m_isFading = true;
                m_fadeTimer = 0.0f;
            }
            goto render_text;
        }

        // ========== AUTO-ADVANCE TIMER ==========
        m_boardTimer += dt;
        {
            bool isLastInPanel = IsLastFrameInPanel(videoComp.activeFrame);
            float autoAdvanceTime = isLastInPanel ? videoComp.panelDuration : videoComp.boardDuration;

            if (m_boardTimer >= autoAdvanceTime)
            {
                // Auto-advance to next frame
                int nextFrame = videoComp.activeFrame + 1;

                if (nextFrame > videoComp.frameEnd)
                {
                    // End of cutscene
                    internalCutsceneEnded = true;
                    isSkipping = false;
                    m_isFading = true;
                    m_fadeTimer = 0.0f;
                }
                else
                {
                    // Start fade to next frame
                    m_isFading = true;
                    m_fadeTimer = 0.0f;
                    m_fadeTargetFrame = nextFrame;
                }
            }
        }

        // ========== TAP INPUT HANDLING ==========
        if (g_inputManager->IsPointerJustPressed() && !m_isFading)
        {
            // Check if text is still typing
            // Use frame number for frame-based dialogue, panel number for panel-based
            int dialogueKey = !videoComp.dialogueMap.empty() ? videoComp.activeFrame : GetPanelForFrame(videoComp.activeFrame);
            bool textFinished = m_dialogueManager.IsTextFinishedForPanel(textComp, dialogueKey);
            textAudioComp.Play();
            if (!textFinished)
            {
                // TAP: Auto-complete typewriter text
                m_dialogueManager.CompleteTextImmediately(textComp, dialogueKey);
            }
            else
            {
                // TAP: Advance to next board
                int nextFrame = videoComp.activeFrame + 1;

                if (nextFrame > videoComp.frameEnd)
                {
                    // End of cutscene
                    internalCutsceneEnded = true;
                    m_isFading = true;
                    m_fadeTimer = 0.0f;
                }
                else
                {
                    // Start fade to next frame
                    m_isFading = true;
                    m_fadeTimer = 0.0f;
                    m_fadeTargetFrame = nextFrame;
                }

                m_boardTimer = 0.0f;  // Reset auto-advance timer on manual advance
            }
        }

render_text:
        // ========== TEXT HANDLING ==========
        // Prefer frame/board-based dialogue for progressive text within panels
        // Fall back to panel-based dialogue if frame-based is not available
        if (!videoComp.dialogueMap.empty())
        {
            // Frame/Board-based dialogue - supports progressive text per board
            m_dialogueManager.dialogueMap = videoComp.dialogueMap;
            m_dialogueManager.HandleTextRender(dt, textComp, textTransform, videoComp.activeFrame);
        }
        else if (!videoComp.panelDialogueMap.empty())
        {
            // Panel-based dialogue - same text for all boards in panel
            m_dialogueManager.panelDialogueMap = videoComp.panelDialogueMap;
            int currentPanel = GetPanelForFrame(videoComp.activeFrame);
            m_dialogueManager.HandlePanelTextRender(dt, textComp, textTransform, currentPanel);
        }
        else
        {
            // No dialogue
            textComp.text = "";
        }
    }
}

void VideoSystem::SwapCutscene(SpriteRenderComponent& comp, std::string newCutscenePath)
{
    comp.texture = ResourceManager::GetInstance().GetResource<Texture>(newCutscenePath);
    comp.texturePath = newCutscenePath;
}

std::string VideoSystem::ConstructNewPath(VideoComponent& videoComp)
{
    std::string numResult = "_" + videoComp.PadNumber(videoComp.activeFrame);
    std::string fileName = videoComp.cutSceneName + numResult + ".png";
    std::string newCutscenePath = rootDirectory + fileName;
    return newCutscenePath;
}

void VideoSystem::FadeInTransition(SpriteRenderComponent& blackScreen, float dt, float duration)
{
    if (duration <= 0) return;

    float progress = dt / duration;
    float easedProgress = easeOutCubic(std::min(progress * 2.0f, 1.0f));
    blackScreen.alpha = lerp(blackScreen.alpha, 0.0f, easedProgress);
}

void VideoSystem::FadeOutTransition(SpriteRenderComponent& blackScreen, float dt, float duration)
{
    if (duration <= 0) return;

    float progress = dt / duration;
    float easedProgress = easeOutCubic(std::min(progress * 2.0f, 1.0f));
    blackScreen.alpha = lerp(blackScreen.alpha, 1.0f, easedProgress);
}
