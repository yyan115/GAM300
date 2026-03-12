#include <pch.h>
#include "Dialogue/DialogueManager.hpp"
#include "Dialogue/DialogueComponent.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Graphics/TextRendering/TextUtils.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "Utilities/GUID.hpp"
#include "Logging.hpp"

NarrativeDialogueManager& NarrativeDialogueManager::GetInstance() {
    static NarrativeDialogueManager instance;
    return instance;
}

void NarrativeDialogueManager::SetECSManager(ECSManager* ecs) {
    m_ecs = ecs;
}

void NarrativeDialogueManager::RegisterDialogue(const std::string& name, Entity entity) {
    if (name.empty()) return;
    m_dialogues[name] = { entity };
}

void NarrativeDialogueManager::UnregisterDialogue(const std::string& name) {
    // If this was the active dialogue, clear active state
    if (m_activeDialogueName == name) {
        m_activeDialogueName.clear();
    }
    m_dialogues.erase(name);
}

void NarrativeDialogueManager::Clear() {
    m_dialogues.clear();
    m_activeDialogueName.clear();
}

Entity NarrativeDialogueManager::GetDialogueEntity(const std::string& name) const {
    auto it = m_dialogues.find(name);
    if (it != m_dialogues.end()) {
        return it->second.entity;
    }
    return INVALID_ENTITY;
}

void NarrativeDialogueManager::StartDialogue(const std::string& name) {
    if (!m_ecs) return;

    auto it = m_dialogues.find(name);
    if (it == m_dialogues.end()) {
        // The dialogue might not be registered yet (or the global manager may have been
        // cleared during an async scene transition). Re-scan active dialogue entities and
        // rebuild any missing registrations on demand.
        auto activeEntities = m_ecs->GetActiveEntities();
        for (Entity e : activeEntities) {
            if (!m_ecs->HasComponent<DialogueComponent>(e)) continue;
            auto& d = m_ecs->GetComponent<DialogueComponent>(e);
            if (d.dialogueName.empty()) continue;

            if (GetDialogueEntity(d.dialogueName) != e) {
                m_dialogues[d.dialogueName] = { e };
            }
            d.registeredWithManager = (GetDialogueEntity(d.dialogueName) == e);

            // Also resolve text entity GUID
            if (d.textEntity == 0 && !d.textEntityGuidStr.empty()) {
                GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(d.textEntityGuidStr);
                d.textEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid);
            }
        }
        // Retry lookup
        it = m_dialogues.find(name);
        if (it == m_dialogues.end()) {
            return;
        }
    }

    // Stop currently active dialogue first (one at a time)
    if (!m_activeDialogueName.empty() && m_activeDialogueName != name) {
        StopDialogue(m_activeDialogueName);
    }

    Entity entity = it->second.entity;
    if (!m_ecs->HasComponent<DialogueComponent>(entity)) return;

    auto& dialogue = m_ecs->GetComponent<DialogueComponent>(entity);

    if (dialogue.entries.empty()) {
        return;
    }

    // Initialize dialogue state
    dialogue.currentIndex = 0;
    dialogue.stateTimer = 0.0f;
    dialogue.typewriterTimer = 0.0f;
    dialogue.revealedChars = 0;
    dialogue.triggerActivated = false;
    dialogue.scrollNextRequested = false;

    // Activate text entity and initialize text component before first frame
    if (dialogue.textEntity != 0) {
        if (m_ecs->HasComponent<ActiveComponent>(dialogue.textEntity)) {
            m_ecs->GetComponent<ActiveComponent>(dialogue.textEntity).isActive = true;
        }
        if (m_ecs->HasComponent<TextRenderComponent>(dialogue.textEntity)) {
            auto& textComp = m_ecs->GetComponent<TextRenderComponent>(dialogue.textEntity);
            auto mode = static_cast<DialogueAppearanceMode>(dialogue.appearanceModeID);

            if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
                textComp.alpha = 0.0f;
                TextUtils::SetText(textComp, dialogue.entries[0].text);
            } else if (mode == DialogueAppearanceMode::Typewriter) {
                textComp.alpha = 1.0f;
                TextUtils::SetText(textComp, "");
            } else {
                textComp.alpha = 1.0f;
                TextUtils::SetText(textComp, dialogue.entries[0].text);
            }
            textComp.isVisible = true;
        }
    }

    // Activate first entry's sprite entity (if any)
    if (!dialogue.entries.empty() && !dialogue.entries[0].spriteEntityGuidStr.empty()) {
        GUID_128 spriteGuid = GUIDUtilities::ConvertStringToGUID128(dialogue.entries[0].spriteEntityGuidStr);
        Entity spriteEnt = EntityGUIDRegistry::GetInstance().GetEntityByGUID(spriteGuid);
        if (spriteEnt != 0) {
            if (m_ecs->HasComponent<ActiveComponent>(spriteEnt)) {
                m_ecs->GetComponent<ActiveComponent>(spriteEnt).isActive = true;
            }
            if (m_ecs->HasComponent<SpriteRenderComponent>(spriteEnt)) {
                auto& spriteComp = m_ecs->GetComponent<SpriteRenderComponent>(spriteEnt);
                auto mode = static_cast<DialogueAppearanceMode>(dialogue.appearanceModeID);
                if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
                    spriteComp.alpha = 0.0f;
                } else {
                    spriteComp.alpha = 1.0f;
                }
                spriteComp.isVisible = true;
            }
        }
    }

    // Determine starting phase based on appearance mode
    auto mode = static_cast<DialogueAppearanceMode>(dialogue.appearanceModeID);
    if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
        dialogue.phase = DialogueComponent::Phase::FadingIn;
    } else if (mode == DialogueAppearanceMode::Typewriter) {
        dialogue.phase = DialogueComponent::Phase::Typing;
    } else {
        dialogue.phase = DialogueComponent::Phase::Displaying;
    }

    m_activeDialogueName = name;
}

void NarrativeDialogueManager::StopDialogue(const std::string& name) {
    if (!m_ecs) return;

    auto it = m_dialogues.find(name);
    if (it == m_dialogues.end()) return;

    Entity entity = it->second.entity;
    if (!m_ecs->HasComponent<DialogueComponent>(entity)) return;

    auto& dialogue = m_ecs->GetComponent<DialogueComponent>(entity);

    // If using fade mode, trigger fade out; otherwise go directly to finished
    auto mode = static_cast<DialogueAppearanceMode>(dialogue.appearanceModeID);
    if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f &&
        dialogue.phase != DialogueComponent::Phase::Inactive &&
        dialogue.phase != DialogueComponent::Phase::Finished) {
        dialogue.phase = DialogueComponent::Phase::FadingOut;
        dialogue.stateTimer = 0.0f;
    } else {
        dialogue.phase = DialogueComponent::Phase::Finished;
    }
}

void NarrativeDialogueManager::ScrollNext(const std::string& name) {
    if (!m_ecs) return;

    auto it = m_dialogues.find(name);
    if (it == m_dialogues.end()) return;

    Entity entity = it->second.entity;
    if (!m_ecs->HasComponent<DialogueComponent>(entity)) return;

    auto& dialogue = m_ecs->GetComponent<DialogueComponent>(entity);

    // Only advance if current entry is set to Action mode
    if (dialogue.currentIndex >= 0 &&
        dialogue.currentIndex < static_cast<int>(dialogue.entries.size())) {
        auto scrollType = static_cast<DialogueScrollType>(
            dialogue.entries[dialogue.currentIndex].scrollTypeID);
        if (scrollType == DialogueScrollType::Action) {
            dialogue.scrollNextRequested = true;
        }
    }
}

bool NarrativeDialogueManager::IsDialogueActive(const std::string& name) const {
    if (!m_ecs) return false;

    auto it = m_dialogues.find(name);
    if (it == m_dialogues.end()) return false;

    Entity entity = it->second.entity;
    if (!m_ecs->HasComponent<DialogueComponent>(entity)) return false;

    const auto& dialogue = m_ecs->GetComponent<DialogueComponent>(entity);
    return dialogue.phase != DialogueComponent::Phase::Inactive &&
           dialogue.phase != DialogueComponent::Phase::Finished;
}

bool NarrativeDialogueManager::IsAnyDialogueActive() const {
    return !m_activeDialogueName.empty() && IsDialogueActive(m_activeDialogueName);
}

int NarrativeDialogueManager::GetCurrentIndex(const std::string& name) const {
    if (!m_ecs) return -1;

    auto it = m_dialogues.find(name);
    if (it == m_dialogues.end()) return -1;

    Entity entity = it->second.entity;
    if (!m_ecs->HasComponent<DialogueComponent>(entity)) return -1;

    const auto& dialogue = m_ecs->GetComponent<DialogueComponent>(entity);
    return dialogue.currentIndex;
}

void NarrativeDialogueManager::OnTriggerEnter(Entity triggerEntity, Entity otherEntity) {
    if (!m_ecs) return;

    // Check all registered dialogues for any that have a trigger entry matching this trigger entity
    for (auto& [name, record] : m_dialogues) {
        if (!m_ecs->HasComponent<DialogueComponent>(record.entity)) continue;

        auto& dialogue = m_ecs->GetComponent<DialogueComponent>(record.entity);

        // Only process if dialogue is active and in a displayable phase
        if (dialogue.phase == DialogueComponent::Phase::Inactive ||
            dialogue.phase == DialogueComponent::Phase::Finished) continue;

        if (triggerEntity == dialogue.textEntity) continue; // Skip text entity matches

        // Only check the CURRENT entry for a matching trigger GUID.
        // Previously this checked ALL entries, which caused a bug: touching trigger A
        // would set triggerActivated even when the current entry expected trigger B,
        // causing dialogue to scroll through all entries uncontrollably.
        if (dialogue.currentIndex < 0 ||
            dialogue.currentIndex >= static_cast<int>(dialogue.entries.size())) continue;

        auto& currentEntry = dialogue.entries[dialogue.currentIndex];
        if (currentEntry.scrollTypeID != static_cast<int>(DialogueScrollType::Trigger)) continue;

        auto& registry = EntityGUIDRegistry::GetInstance();
        GUID_128 triggerGuid = registry.GetGUIDByEntity(triggerEntity);
        std::string triggerGuidStr = GUIDUtilities::ConvertGUID128ToString(triggerGuid);

        if (!currentEntry.triggerEntityGuidStr.empty() &&
            triggerGuidStr == currentEntry.triggerEntityGuidStr) {
            dialogue.triggerActivated = true;
        }
    }
}
