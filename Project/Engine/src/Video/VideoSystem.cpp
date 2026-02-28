#include "pch.h"
#include "Video/VideoSystem.hpp"
#include "Video/VideoComponent.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "Utilities/GUID.hpp"
#include "Input/InputManager.h"
#include "Scene/SceneManager.hpp"
#include "Logging.hpp"
#include <algorithm>
#include <cmath>

// ============================================================================
// Helpers
// ============================================================================

float VideoSystem::Smoothstep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ============================================================================
// Init
// ============================================================================

void VideoSystem::Initialise(ECSManager& ecsManager)
{
    m_ecs = &ecsManager;
}

// ============================================================================
// Entity Resolution (GUID → Entity, once at init)
// ============================================================================

void VideoSystem::ResolveEntityReferences(VideoComponent& vc)
{
    auto resolve = [](const std::string& guidStr) -> Entity {
        if (guidStr.empty()) return 0;
        GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
        return EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid);
    };

    vc.textEntity = resolve(vc.textEntityGuidStr);
    vc.blackScreenEntity = resolve(vc.blackScreenEntityGuidStr);
    vc.skipButtonEntity = resolve(vc.skipButtonEntityGuidStr);
}

// ============================================================================
// Cutscene Lifecycle
// ============================================================================

void VideoSystem::BeginCutscene(VideoComponent& vc)
{
    if (vc.boards.empty()) return;

    vc.currentBoardIndex = 0;
    vc.stateTimer = 0.0f;
    vc.typewriterTimer = 0.0f;
    vc.revealedChars = 0;
    vc.previousBoardChars = 0;
    vc.cutsceneEnded = false;
    vc.skipRequested = false;

    // Set initial board image
    SwapBoardImage(vc, 0);

    // Start with black screen, then fade in
    SetBlackScreenAlpha(vc, 1.0f);
    vc.phase = VideoComponent::Phase::FadingIn;
}

void VideoSystem::AdvanceToBoard(VideoComponent& vc, int boardIndex)
{
    if (boardIndex >= static_cast<int>(vc.boards.size()))
    {
        BeginEndingFade(vc);
        return;
    }

    vc.stateTimer = 0.0f;
    vc.phase = VideoComponent::Phase::TransitionOut;
}

void VideoSystem::BeginEndingFade(VideoComponent& vc)
{
    vc.stateTimer = 0.0f;
    vc.phase = VideoComponent::Phase::EndingFade;
}

void VideoSystem::FinishCutscene(VideoComponent& vc)
{
    vc.cutsceneEnded = true;
    vc.phase = VideoComponent::Phase::Finished;
    ClearText(vc);

    // Reset blur
    BlurEffect* blur = PostProcessingManager::GetInstance().GetBlurEffect();
    if (blur) blur->SetIntensity(0.0f);
}

// ============================================================================
// Typewriter
// ============================================================================

void VideoSystem::UpdateTypewriter(VideoComponent& vc, float dt)
{
    if (vc.currentBoardIndex < 0 || vc.currentBoardIndex >= static_cast<int>(vc.boards.size()))
        return;
    if (vc.textEntity == 0 || !m_ecs->HasComponent<TextRenderComponent>(vc.textEntity))
        return;

    const CutsceneBoard& board = vc.boards[vc.currentBoardIndex];
    auto& textComp = m_ecs->GetComponent<TextRenderComponent>(vc.textEntity);

    if (board.text.empty())
    {
        textComp.text = "";
        return;
    }

    if (board.textSpeed <= 0.0f)
    {
        // Instant display
        textComp.text = board.text;
        vc.revealedChars = static_cast<int>(board.text.size());
        return;
    }

    vc.typewriterTimer += dt;
    int totalChars = vc.previousBoardChars + static_cast<int>(vc.typewriterTimer * board.textSpeed);
    totalChars = std::min(totalChars, static_cast<int>(board.text.size()));
    vc.revealedChars = totalChars;
    textComp.text = board.text.substr(0, totalChars);
}

void VideoSystem::CompleteTypewriter(VideoComponent& vc)
{
    if (vc.currentBoardIndex < 0 || vc.currentBoardIndex >= static_cast<int>(vc.boards.size()))
        return;
    if (vc.textEntity == 0 || !m_ecs->HasComponent<TextRenderComponent>(vc.textEntity))
        return;

    const CutsceneBoard& board = vc.boards[vc.currentBoardIndex];
    auto& textComp = m_ecs->GetComponent<TextRenderComponent>(vc.textEntity);

    textComp.text = board.text;
    vc.revealedChars = static_cast<int>(board.text.size());
    vc.typewriterTimer = static_cast<float>(board.text.size()) / std::max(board.textSpeed, 1.0f);
}

bool VideoSystem::IsTypewriterFinished(const VideoComponent& vc) const
{
    if (vc.currentBoardIndex < 0 || vc.currentBoardIndex >= static_cast<int>(vc.boards.size()))
        return true;
    const CutsceneBoard& board = vc.boards[vc.currentBoardIndex];
    if (board.text.empty()) return true;
    return vc.revealedChars >= static_cast<int>(board.text.size());
}

// ============================================================================
// Blur
// ============================================================================

void VideoSystem::ApplyBlur(const VideoComponent& vc)
{
    if (vc.currentBoardIndex < 0 || vc.currentBoardIndex >= static_cast<int>(vc.boards.size()))
        return;

    const CutsceneBoard& board = vc.boards[vc.currentBoardIndex];
    BlurEffect* blur = PostProcessingManager::GetInstance().GetBlurEffect();
    if (!blur) return;

    float intensity = board.blurIntensity;

    // Interpolate between start and end if blurIntensityEnd is set (>= 0)
    if (board.blurIntensityEnd >= 0.0f && board.duration > 0.0f)
    {
        float t = std::clamp(vc.stateTimer / board.duration, 0.0f, 1.0f);
        intensity = board.blurIntensity + (board.blurIntensityEnd - board.blurIntensity) * t;
    }

    blur->SetIntensity(intensity);
    blur->SetRadius(board.blurRadius);
    blur->SetPasses(board.blurPasses);
}

// ============================================================================
// Image Swap & UI helpers
// ============================================================================

void VideoSystem::SwapBoardImage(VideoComponent& vc, int boardIndex)
{
    if (boardIndex < 0 || boardIndex >= static_cast<int>(vc.boards.size()))
        return;

    // The VideoComponent entity itself should have a SpriteRenderComponent
    // (the entity this component is on)
    for (const auto& entity : entities)
    {
        if (!m_ecs->HasComponent<VideoComponent>(entity)) continue;
        auto& thisVc = m_ecs->GetComponent<VideoComponent>(entity);
        if (&thisVc != &vc) continue;

        if (!m_ecs->HasComponent<SpriteRenderComponent>(entity)) break;

        auto& sprite = m_ecs->GetComponent<SpriteRenderComponent>(entity);
        std::string loadPath = vc.boards[boardIndex].imagePath;

#ifdef ANDROID
        // Strip leading ../../ for Android asset loading
        while (loadPath.size() >= 3 && loadPath.substr(0, 3) == "../")
            loadPath = loadPath.substr(3);
#endif

        if (!loadPath.empty())
        {
            sprite.texture = ResourceManager::GetInstance().GetResource<Texture>(loadPath);
            sprite.texturePath = loadPath;
        }
        break;
    }
}

void VideoSystem::SetBlackScreenAlpha(VideoComponent& vc, float alpha)
{
    if (vc.blackScreenEntity == 0) return;
    if (!m_ecs->HasComponent<SpriteRenderComponent>(vc.blackScreenEntity)) return;
    m_ecs->GetComponent<SpriteRenderComponent>(vc.blackScreenEntity).alpha = alpha;
}

void VideoSystem::ClearText(VideoComponent& vc)
{
    if (vc.textEntity == 0) return;
    if (!m_ecs->HasComponent<TextRenderComponent>(vc.textEntity)) return;
    m_ecs->GetComponent<TextRenderComponent>(vc.textEntity).text = "";
}

// ============================================================================
// Main Update Loop
// ============================================================================

void VideoSystem::Update(float dt)
{
    PROFILE_FUNCTION();

    for (const auto& entity : entities)
    {
        if (!m_ecs->HasComponent<VideoComponent>(entity))
            continue;
        if (!m_ecs->IsEntityActiveInHierarchy(entity))
            continue;

        auto& vc = m_ecs->GetComponent<VideoComponent>(entity);

        if (!vc.enabled) continue;

        // One-time GUID resolution
        if (vc.needsInit)
        {
            ResolveEntityReferences(vc);
            if (vc.autoStart && !vc.boards.empty())
            {
                if (vc.startDelay > 0.0f)
                {
                    vc.startDelayTimer = vc.startDelay;
                    vc.phase = VideoComponent::Phase::Inactive;
                }
                else
                {
                    BeginCutscene(vc);
                }
            }
            vc.needsInit = false;
            continue;
        }

        // Wait for start delay
        if (vc.startDelayTimer > 0.0f)
        {
            vc.startDelayTimer -= dt;
            if (vc.startDelayTimer <= 0.0f)
            {
                vc.startDelayTimer = 0.0f;
                BeginCutscene(vc);
            }
            continue;
        }

        // Handle external skip request
        if (vc.skipRequested && vc.phase != VideoComponent::Phase::EndingFade
            && vc.phase != VideoComponent::Phase::Finished)
        {
            vc.skipRequested = false;
            BeginEndingFade(vc);
        }

        // Always apply blur for active cutscenes
        if (vc.phase != VideoComponent::Phase::Inactive && vc.phase != VideoComponent::Phase::Finished)
            ApplyBlur(vc);

        switch (vc.phase)
        {
        case VideoComponent::Phase::Inactive:
            break;

        case VideoComponent::Phase::FadingIn:
        {
            if (vc.boards.empty()) break;
            float fadeDur = vc.boards[0].fadeDuration;
            if (fadeDur <= 0.0f) fadeDur = 0.01f;

            vc.stateTimer += dt;
            float progress = std::clamp(vc.stateTimer / fadeDur, 0.0f, 1.0f);
            SetBlackScreenAlpha(vc, 1.0f - Smoothstep(progress));
            UpdateTypewriter(vc, dt);

            if (progress >= 1.0f)
            {
                SetBlackScreenAlpha(vc, 0.0f);
                vc.phase = VideoComponent::Phase::Displaying;
                vc.stateTimer = 0.0f;
            }
            break;
        }

        case VideoComponent::Phase::Displaying:
        {
            if (vc.currentBoardIndex < 0 || vc.currentBoardIndex >= static_cast<int>(vc.boards.size()))
                break;

            const CutsceneBoard& board = vc.boards[vc.currentBoardIndex];
            vc.stateTimer += dt;
            UpdateTypewriter(vc, dt);

            // Tap input
            if (g_inputManager->IsPointerJustPressed())
            {
                if (!IsTypewriterFinished(vc))
                {
                    CompleteTypewriter(vc);
                }
                else
                {
                    int nextBoard = vc.currentBoardIndex + 1;
                    if (nextBoard < static_cast<int>(vc.boards.size()))
                    {
                        AdvanceToBoard(vc, nextBoard);
                    }
                    else
                    {
                        BeginEndingFade(vc);
                    }
                    vc.stateTimer = 0.0f;
                }
            }

            // Auto-advance
            if (vc.stateTimer >= board.duration && vc.phase == VideoComponent::Phase::Displaying)
            {
                int nextBoard = vc.currentBoardIndex + 1;
                if (nextBoard < static_cast<int>(vc.boards.size()))
                    AdvanceToBoard(vc, nextBoard);
                else
                    BeginEndingFade(vc);
            }
            break;
        }

        case VideoComponent::Phase::TransitionOut:
        {
            int nextBoard = vc.currentBoardIndex + 1;
            if (nextBoard >= static_cast<int>(vc.boards.size()))
            {
                BeginEndingFade(vc);
                break;
            }

            float fadeDur = vc.boards[nextBoard].fadeDuration;
            if (fadeDur <= 0.0f) fadeDur = 0.01f;
            float halfFade = fadeDur * 0.5f;

            vc.stateTimer += dt;
            float progress = std::clamp(vc.stateTimer / halfFade, 0.0f, 1.0f);
            SetBlackScreenAlpha(vc, Smoothstep(progress));

            if (progress >= 1.0f)
            {
                // Swap image at midpoint
                SwapBoardImage(vc, nextBoard);

                // Handle typewriter continuation
                if (vc.boards[nextBoard].continueText)
                {
                    vc.previousBoardChars = vc.revealedChars;
                    // Don't reset typewriterTimer — continue from where we left off
                }
                else
                {
                    vc.previousBoardChars = 0;
                    vc.typewriterTimer = 0.0f;
                    vc.revealedChars = 0;
                }

                vc.currentBoardIndex = nextBoard;
                vc.stateTimer = 0.0f;
                vc.phase = VideoComponent::Phase::TransitionIn;
            }
            break;
        }

        case VideoComponent::Phase::TransitionIn:
        {
            if (vc.currentBoardIndex < 0 || vc.currentBoardIndex >= static_cast<int>(vc.boards.size()))
                break;

            float fadeDur = vc.boards[vc.currentBoardIndex].fadeDuration;
            if (fadeDur <= 0.0f) fadeDur = 0.01f;
            float halfFade = fadeDur * 0.5f;

            vc.stateTimer += dt;
            float progress = std::clamp(vc.stateTimer / halfFade, 0.0f, 1.0f);
            SetBlackScreenAlpha(vc, 1.0f - Smoothstep(progress));
            UpdateTypewriter(vc, dt);

            if (progress >= 1.0f)
            {
                SetBlackScreenAlpha(vc, 0.0f);
                vc.phase = VideoComponent::Phase::Displaying;
                vc.stateTimer = 0.0f;
            }
            break;
        }

        case VideoComponent::Phase::EndingFade:
        {
            float fadeDur = vc.skipFadeDuration;
            if (fadeDur <= 0.0f) fadeDur = 0.01f;

            vc.stateTimer += dt;
            float progress = std::clamp(vc.stateTimer / fadeDur, 0.0f, 1.0f);
            SetBlackScreenAlpha(vc, Smoothstep(progress));

            // Hide skip button during ending
            if (vc.skipButtonEntity != 0 && m_ecs->HasComponent<SpriteRenderComponent>(vc.skipButtonEntity))
                m_ecs->GetComponent<SpriteRenderComponent>(vc.skipButtonEntity).isVisible = false;

            if (progress >= 1.0f)
            {
                SetBlackScreenAlpha(vc, 1.0f);
                FinishCutscene(vc);
            }
            break;
        }

        case VideoComponent::Phase::Finished:
        {
            if (!vc.nextScenePath.empty())
            {
                SceneManager::GetInstance().LoadScene(vc.nextScenePath);
            }
            else if (vc.loop)
            {
                BeginCutscene(vc);
            }
            break;
        }

        } // switch
    } // for entities
}
