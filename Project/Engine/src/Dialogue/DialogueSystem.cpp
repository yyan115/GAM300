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
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Logging.hpp"
#include <algorithm>

// === Sprite entity helpers (per-entry) ===

static Entity ResolveSpriteEntity(const DialogueEntry& entry, ECSManager* ecs) {
    if (entry.spriteEntityGuidStr.empty() || !ecs) return 0;
    GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(entry.spriteEntityGuidStr);
    return EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid);
}

static SpriteRenderComponent* GetSpriteComp(Entity spriteEntity, ECSManager* ecs) {
    if (spriteEntity == 0 || !ecs) return nullptr;
    if (!ecs->HasComponent<SpriteRenderComponent>(spriteEntity)) return nullptr;
    return &ecs->GetComponent<SpriteRenderComponent>(spriteEntity);
}

static void ActivateSpriteEntity(Entity spriteEntity, ECSManager* ecs) {
    if (spriteEntity == 0 || !ecs) return;
    if (ecs->HasComponent<ActiveComponent>(spriteEntity)) {
        ecs->GetComponent<ActiveComponent>(spriteEntity).isActive = true;
    }
}

static void DeactivateSpriteEntity(Entity spriteEntity, ECSManager* ecs) {
    if (spriteEntity == 0 || !ecs) return;
    auto* spriteComp = GetSpriteComp(spriteEntity, ecs);
    if (spriteComp) {
        spriteComp->alpha = 0.0f;
        spriteComp->isVisible = false;
    }
    if (ecs->HasComponent<ActiveComponent>(spriteEntity)) {
        ecs->GetComponent<ActiveComponent>(spriteEntity).isActive = false;
    }
}

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
                    TextUtils::SetText(*textComp, entry.text);
                    textComp->isVisible = true;
                }
            }

            // Fade sprite alpha in sync with text
            if (dialogue.currentIndex >= 0 &&
                dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                Entity spriteEnt = ResolveSpriteEntity(dialogue.entries[dialogue.currentIndex], m_ecs);
                auto* spriteComp = GetSpriteComp(spriteEnt, m_ecs);
                if (spriteComp) {
                    spriteComp->alpha = progress;
                }
            }

            if (progress >= 1.0f) {
                dialogue.phase = DialogueComponent::Phase::Displaying;
                dialogue.stateTimer = 0.0f;
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

            // Commented out as not used to fix warnings.
            // auto scrollType = static_cast<DialogueScrollType>(entry.scrollTypeID);
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

            // Fade sprite alpha in sync with text
            if (dialogue.currentIndex >= 0 &&
                dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                Entity spriteEnt = ResolveSpriteEntity(dialogue.entries[dialogue.currentIndex], m_ecs);
                auto* spriteComp = GetSpriteComp(spriteEnt, m_ecs);
                if (spriteComp) {
                    spriteComp->alpha = 1.0f - progress;
                }
            }

            if (progress >= 1.0f) {
                // Deactivate current entry's sprite before transitioning
                if (dialogue.currentIndex >= 0 &&
                    dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                    Entity spriteEnt = ResolveSpriteEntity(dialogue.entries[dialogue.currentIndex], m_ecs);
                    DeactivateSpriteEntity(spriteEnt, m_ecs);
                }

                int nextIndex = dialogue.currentIndex + 1;
                if (nextIndex < static_cast<int>(dialogue.entries.size())) {
                    // More entries remaining - advance and fade in the next one
                    dialogue.currentIndex = nextIndex;
                    BeginEntry(dialogue); // will set FadingIn phase
                } else {
                    // Last entry done - end dialogue
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
            // Clean up current entry's sprite entity
            if (dialogue.currentIndex >= 0 &&
                dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                Entity spriteEnt = ResolveSpriteEntity(dialogue.entries[dialogue.currentIndex], m_ecs);
                DeactivateSpriteEntity(spriteEnt, m_ecs);
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
    auto mode = static_cast<DialogueAppearanceMode>(dialogue.appearanceModeID);

    // FadeInOut mode: fade out current entry first, then FadingOut handler
    // will either advance to next entry or end dialogue
    if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
        dialogue.phase = DialogueComponent::Phase::FadingOut;
        dialogue.stateTimer = 0.0f;
        return;
    }

    // Non-fade modes: instant transition
    // Deactivate the current entry's sprite entity before advancing
    if (dialogue.currentIndex >= 0 &&
        dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
        Entity prevSprite = ResolveSpriteEntity(dialogue.entries[dialogue.currentIndex], m_ecs);
        DeactivateSpriteEntity(prevSprite, m_ecs);
    }

    int nextIndex = dialogue.currentIndex + 1;
    if (nextIndex >= static_cast<int>(dialogue.entries.size())) {
        EndDialogue(dialogue);
        return;
    }

    dialogue.currentIndex = nextIndex;
    BeginEntry(dialogue);
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

    // Commented out as not used to fix warnings.
    // bool isFirstEntry = (dialogue.currentIndex == 0);

    // Resolve and activate sprite entity for this entry
    Entity spriteEnt = 0;
    SpriteRenderComponent* spriteComp = nullptr;
    if (dialogue.currentIndex >= 0 &&
        dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
        spriteEnt = ResolveSpriteEntity(dialogue.entries[dialogue.currentIndex], m_ecs);
        if (spriteEnt != 0) {
            ActivateSpriteEntity(spriteEnt, m_ecs);
            spriteComp = GetSpriteComp(spriteEnt, m_ecs);
        }
    }

    if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
        // Fade in this entry
        dialogue.phase = DialogueComponent::Phase::FadingIn;
        if (textComp) {
            textComp->alpha = 0.0f;
            if (dialogue.currentIndex >= 0 &&
                dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                TextUtils::SetText(*textComp, dialogue.entries[dialogue.currentIndex].text);
            }
            textComp->isVisible = true;
        }
        if (spriteComp) {
            spriteComp->alpha = 0.0f;
            spriteComp->isVisible = true;
        }
    } else if (mode == DialogueAppearanceMode::Typewriter) {
        dialogue.phase = DialogueComponent::Phase::Typing;
        if (textComp) {
            textComp->alpha = 1.0f;
            textComp->isVisible = true;
            TextUtils::SetText(*textComp, "");
        }
        if (spriteComp) {
            spriteComp->alpha = 1.0f;
            spriteComp->isVisible = true;
        }
    } else {
        // Instant
        dialogue.phase = DialogueComponent::Phase::Displaying;
        if (textComp) {
            textComp->alpha = 1.0f;
            textComp->isVisible = true;
            if (dialogue.currentIndex >= 0 &&
                dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
                TextUtils::SetText(*textComp, dialogue.entries[dialogue.currentIndex].text);
            }
        }
        if (spriteComp) {
            spriteComp->alpha = 1.0f;
            spriteComp->isVisible = true;
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

    // Deactivate current entry's sprite entity
    if (dialogue.currentIndex >= 0 &&
        dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
        Entity spriteEnt = ResolveSpriteEntity(dialogue.entries[dialogue.currentIndex], m_ecs);
        DeactivateSpriteEntity(spriteEnt, m_ecs);
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
