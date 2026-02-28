#include <pch.h>
#include "Dialogue/DialogueManager.hpp"
#include "Dialogue/DialogueComponent.hpp"
#include "ECS/ECSManager.hpp"
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
    return 0;
}

void NarrativeDialogueManager::StartDialogue(const std::string& name) {
    if (!m_ecs) return;

    auto it = m_dialogues.find(name);
    if (it == m_dialogues.end()) {
        // Lazy registration: scan all entities for unregistered DialogueComponents
        for (auto& [existingName, record] : m_dialogues) { /* already registered */ }
        // The dialogue might not be registered yet (script Start() ran before system Update)
        // Scan all entities with DialogueComponent and register them now
        auto activeEntities = m_ecs->GetActiveEntities();
        for (Entity e : activeEntities) {
            if (!m_ecs->HasComponent<DialogueComponent>(e)) continue;
            auto& d = m_ecs->GetComponent<DialogueComponent>(e);
            if (!d.dialogueName.empty() && !d.registeredWithManager) {
                m_dialogues[d.dialogueName] = { e };
                d.registeredWithManager = true;
                // Also resolve text entity GUID
                if (d.textEntity == 0 && !d.textEntityGuidStr.empty()) {
                    GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(d.textEntityGuidStr);
                    d.textEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid);
                }
            }
        }
        // Retry lookup
        it = m_dialogues.find(name);
        if (it == m_dialogues.end()) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn,
                "[NarrativeDialogueManager] Dialogue not found: ", name);
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
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[NarrativeDialogueManager] Dialogue has no entries: ", name);
        return;
    }

    // Initialize dialogue state
    dialogue.currentIndex = 0;
    dialogue.stateTimer = 0.0f;
    dialogue.typewriterTimer = 0.0f;
    dialogue.revealedChars = 0;
    dialogue.triggerActivated = false;
    dialogue.scrollNextRequested = false;

    // Determine starting phase based on appearance mode
    auto mode = static_cast<DialogueAppearanceMode>(dialogue.appearanceModeID);
    if (mode == DialogueAppearanceMode::FadeInOut && dialogue.fadeDuration > 0.0f) {
        dialogue.phase = DialogueComponent::Phase::FadingIn;
    } else if (dialogue.typewriterEnabled) {
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

        // Check ALL entries for a matching trigger GUID (not just current entry).
        // This allows a trigger to skip/advance even if the current entry uses Time scroll.
        auto& registry = EntityGUIDRegistry::GetInstance();
        GUID_128 triggerGuid = registry.GetGUIDByEntity(triggerEntity);
        std::string triggerGuidStr = GUIDUtilities::ConvertGUID128ToString(triggerGuid);

        for (auto& entry : dialogue.entries) {
            if (entry.scrollTypeID != static_cast<int>(DialogueScrollType::Trigger)) continue;
            if (!entry.triggerEntityGuidStr.empty() && triggerGuidStr == entry.triggerEntityGuidStr) {
                dialogue.triggerActivated = true;
                break;
            }
        }
    }
}
