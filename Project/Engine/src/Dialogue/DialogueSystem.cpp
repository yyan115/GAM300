#include <pch.h>
#include "Dialogue/DialogueSystem.hpp"
#include "Dialogue/DialogueComponent.hpp"
#include "Dialogue/DialogueManager.hpp"
#include "ECS/ECSManager.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Graphics/TextRendering/TextUtils.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "Utilities/GUID.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Logging.hpp"
#include <algorithm>

void DialogueSystem::Initialise(ECSManager& ecsManager) {
    m_ecs = &ecsManager;
    auto& manager = NarrativeDialogueManager::GetInstance();
    manager.SetECSManager(m_ecs);

    // Pre-register all dialogues so scripts can reference them immediately in Start()
    for (auto entity : entities) {
        if (!m_ecs->HasComponent<DialogueComponent>(entity)) continue;
        auto& dialogue = m_ecs->GetComponent<DialogueComponent>(entity);
        if (!dialogue.dialogueName.empty()) {
            manager.RegisterDialogue(dialogue.dialogueName, entity);
            dialogue.registeredWithManager = true;
        }
        // Also resolve text entity GUID early
        if (dialogue.textEntity == 0 && !dialogue.textEntityGuidStr.empty()) {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(dialogue.textEntityGuidStr);
            dialogue.textEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid);
        }
    }
}

void DialogueSystem::Update(float dt) {
    if (!m_ecs) return;

    auto& manager = NarrativeDialogueManager::GetInstance();

    for (auto entity : entities) {
        if (!m_ecs->IsEntityActiveInHierarchy(entity)) continue;

        auto& dialogue = m_ecs->GetComponent<DialogueComponent>(entity);

        // Register with manager if not yet done
        if (!dialogue.registeredWithManager && !dialogue.dialogueName.empty()) {
            manager.RegisterDialogue(dialogue.dialogueName, entity);
            dialogue.registeredWithManager = true;
        }

        // Resolve text entity from GUID if needed
        if (dialogue.textEntity == 0 && !dialogue.textEntityGuidStr.empty()) {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(dialogue.textEntityGuidStr);
            dialogue.textEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid);
        }

        // Auto-start on first frame if enabled
        if (dialogue.autoStart && dialogue.phase == DialogueComponent::Phase::Inactive && !dialogue.autoStartFired) {
            dialogue.autoStartFired = true;
            manager.StartDialogue(dialogue.dialogueName);
        }

        // Skip if inactive
        if (dialogue.phase == DialogueComponent::Phase::Inactive) continue;

        // Activate text entity if it's inactive (dialogue just started)
        if (dialogue.textEntity != 0 && m_ecs->HasComponent<ActiveComponent>(dialogue.textEntity)) {
            auto& activeComp = m_ecs->GetComponent<ActiveComponent>(dialogue.textEntity);
            if (!activeComp.isActive) {
                activeComp.isActive = true;
            }
        }

        // Get text render component if available
        TextRenderComponent* textComp = nullptr;
        if (dialogue.textEntity != 0 && m_ecs->HasComponent<TextRenderComponent>(dialogue.textEntity)) {
            textComp = &m_ecs->GetComponent<TextRenderComponent>(dialogue.textEntity);
        }

        switch (dialogue.phase) {

        case DialogueComponent::Phase::FadingIn: {
            dialogue.stateTimer += dt;
            float progress = dialogue.fadeDuration > 0.0f
                ? std::min(dialogue.stateTimer / dialogue.fadeDuration, 1.0f)
                : 1.0f;

            if (textComp) {
                textComp->alpha = progress;

                // Set text content on first frame of fade-in
                if (dialogue.stateTimer <= dt && dialogue.currentIndex >= 0 &&
                    dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                    auto& entry = dialogue.entries[dialogue.currentIndex];
                    if (dialogue.typewriterEnabled) {
                        TextUtils::SetText(*textComp, "");
                        dialogue.revealedChars = 0;
                        dialogue.typewriterTimer = 0.0f;
                    } else {
                        TextUtils::SetText(*textComp, entry.text);
                    }
                    textComp->isVisible = true;
                }
            }

            if (progress >= 1.0f) {
                if (dialogue.typewriterEnabled) {
                    dialogue.phase = DialogueComponent::Phase::Typing;
                    dialogue.typewriterTimer = 0.0f;
                    dialogue.revealedChars = 0;
                } else {
                    dialogue.phase = DialogueComponent::Phase::Displaying;
                    dialogue.stateTimer = 0.0f;
                }
            }
            break;
        }

        case DialogueComponent::Phase::Typing: {
            if (dialogue.currentIndex < 0 ||
                dialogue.currentIndex >= static_cast<int>(dialogue.entries.size())) {
                dialogue.phase = DialogueComponent::Phase::Displaying;
                break;
            }

            auto& entry = dialogue.entries[dialogue.currentIndex];
            int totalChars = static_cast<int>(entry.text.size());

            dialogue.typewriterTimer += dt;
            int charsToReveal = static_cast<int>(dialogue.typewriterTimer * dialogue.textSpeed);
            charsToReveal = std::min(charsToReveal, totalChars);

            if (charsToReveal > dialogue.revealedChars) {
                dialogue.revealedChars = charsToReveal;
                if (textComp) {
                    TextUtils::SetText(*textComp, entry.text.substr(0, dialogue.revealedChars));
                }
            }

            // Check if Action/Trigger/ScrollNext can skip typing
            auto scrollType = static_cast<DialogueScrollType>(entry.scrollTypeID);
            if (dialogue.scrollNextRequested) {
                if (textComp) TextUtils::SetText(*textComp, entry.text);
                dialogue.scrollNextRequested = false;
                AdvanceToNextEntry(dialogue);
                break;
            }
            if (dialogue.triggerActivated) {
                if (textComp) TextUtils::SetText(*textComp, entry.text);
                dialogue.triggerActivated = false;
                AdvanceToNextEntry(dialogue);
                break;
            }

            // All characters revealed -> move to displaying
            if (dialogue.revealedChars >= totalChars) {
                dialogue.phase = DialogueComponent::Phase::Displaying;
                dialogue.stateTimer = 0.0f;
            }
            break;
        }

        case DialogueComponent::Phase::Displaying: {
            if (dialogue.currentIndex < 0 ||
                dialogue.currentIndex >= static_cast<int>(dialogue.entries.size())) {
                EndDialogue(dialogue);
                break;
            }

            auto& entry = dialogue.entries[dialogue.currentIndex];
            auto scrollType = static_cast<DialogueScrollType>(entry.scrollTypeID);

            switch (scrollType) {
            case DialogueScrollType::Time:
                dialogue.stateTimer += dt;
                if (dialogue.stateTimer >= entry.autoTime) {
                    AdvanceToNextEntry(dialogue);
                }
                break;

            case DialogueScrollType::Action:
                if (dialogue.scrollNextRequested) {
                    dialogue.scrollNextRequested = false;
                    AdvanceToNextEntry(dialogue);
                }
                break;

            case DialogueScrollType::Trigger:
                if (dialogue.triggerActivated) {
                    dialogue.triggerActivated = false;
                    AdvanceToNextEntry(dialogue);
                }
                break;
            }
            break;
        }

        case DialogueComponent::Phase::FadingOut: {
            dialogue.stateTimer += dt;
            float progress = dialogue.fadeDuration > 0.0f
                ? std::min(dialogue.stateTimer / dialogue.fadeDuration, 1.0f)
                : 1.0f;

            if (textComp) {
                textComp->alpha = 1.0f - progress;
            }

            if (progress >= 1.0f) {
                // Check if we're fading out between entries or ending
                int nextIndex = dialogue.currentIndex + 1;
                if (nextIndex < static_cast<int>(dialogue.entries.size())) {
                    // Move to next entry
                    dialogue.currentIndex = nextIndex;
                    BeginEntry(dialogue);
                } else {
                    // Dialogue is done
                    EndDialogue(dialogue);
                }
            }
            break;
        }

        case DialogueComponent::Phase::Finished: {
            // Clean up
            if (textComp) {
                textComp->alpha = 0.0f;
                textComp->isVisible = false;
            }
            dialogue.phase = DialogueComponent::Phase::Inactive;
            dialogue.currentIndex = -1;

            // Clear active dialogue in manager
            if (manager.IsDialogueActive(dialogue.dialogueName)) {
                // The manager will clear its active name when it detects inactive state
            }
            break;
        }

        default:
            break;
        }
    }
}

void DialogueSystem::AdvanceToNextEntry(DialogueComponent& dialogue) {
    int nextIndex = dialogue.currentIndex + 1;
    auto mode = static_cast<DialogueAppearanceMode>(dialogue.appearanceModeID);

    if (nextIndex >= static_cast<int>(dialogue.entries.size())) {
        // This was the last entry - end the dialogue
        if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
            dialogue.phase = DialogueComponent::Phase::FadingOut;
            dialogue.stateTimer = 0.0f;
        } else {
            EndDialogue(dialogue);
        }
        return;
    }

    // Transition to next entry
    if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
        // Fade out current, then fade in next (handled in FadingOut phase)
        dialogue.phase = DialogueComponent::Phase::FadingOut;
        dialogue.stateTimer = 0.0f;
    } else {
        // Instant mode - directly go to next entry
        dialogue.currentIndex = nextIndex;
        BeginEntry(dialogue);
    }
}

void DialogueSystem::BeginEntry(DialogueComponent& dialogue) {
    dialogue.stateTimer = 0.0f;
    dialogue.typewriterTimer = 0.0f;
    dialogue.revealedChars = 0;
    dialogue.triggerActivated = false;
    dialogue.scrollNextRequested = false;

    auto mode = static_cast<DialogueAppearanceMode>(dialogue.appearanceModeID);

    // Set text on the text entity
    TextRenderComponent* textComp = nullptr;
    if (dialogue.textEntity != 0 && m_ecs &&
        m_ecs->HasComponent<TextRenderComponent>(dialogue.textEntity)) {
        textComp = &m_ecs->GetComponent<TextRenderComponent>(dialogue.textEntity);
    }

    if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
        dialogue.phase = DialogueComponent::Phase::FadingIn;
        if (textComp) {
            textComp->alpha = 0.0f;
            if (dialogue.typewriterEnabled) {
                TextUtils::SetText(*textComp, "");
            } else if (dialogue.currentIndex >= 0 &&
                       dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                TextUtils::SetText(*textComp, dialogue.entries[dialogue.currentIndex].text);
            }
            textComp->isVisible = true;
        }
    } else {
        // Instant or no fade
        if (textComp) {
            textComp->alpha = 1.0f;
            textComp->isVisible = true;
        }

        if (dialogue.typewriterEnabled) {
            dialogue.phase = DialogueComponent::Phase::Typing;
            if (textComp) TextUtils::SetText(*textComp, "");
        } else {
            dialogue.phase = DialogueComponent::Phase::Displaying;
            if (textComp && dialogue.currentIndex >= 0 &&
                dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                TextUtils::SetText(*textComp, dialogue.entries[dialogue.currentIndex].text);
            }
        }
    }
}

void DialogueSystem::EndDialogue(DialogueComponent& dialogue) {
    TextRenderComponent* textComp = nullptr;
    if (dialogue.textEntity != 0 && m_ecs &&
        m_ecs->HasComponent<TextRenderComponent>(dialogue.textEntity)) {
        textComp = &m_ecs->GetComponent<TextRenderComponent>(dialogue.textEntity);
    }

    if (textComp) {
        textComp->alpha = 0.0f;
        textComp->isVisible = false;
        TextUtils::SetText(*textComp, "");
    }

    // Deactivate the text entity
    if (dialogue.textEntity != 0 && m_ecs &&
        m_ecs->HasComponent<ActiveComponent>(dialogue.textEntity)) {
        m_ecs->GetComponent<ActiveComponent>(dialogue.textEntity).isActive = false;
    }

    dialogue.phase = DialogueComponent::Phase::Inactive;
    dialogue.currentIndex = -1;
    dialogue.stateTimer = 0.0f;
    dialogue.typewriterTimer = 0.0f;
    dialogue.revealedChars = 0;
    dialogue.triggerActivated = false;
    dialogue.scrollNextRequested = false;
}

void DialogueSystem::Shutdown() {
    NarrativeDialogueManager::GetInstance().Clear();
    m_ecs = nullptr;
}
