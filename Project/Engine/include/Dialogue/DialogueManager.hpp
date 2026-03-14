#pragma once
#include <string>
#include <unordered_map>
#include "ECS/Entity.hpp"

class ECSManager;

class NarrativeDialogueManager {
public:
    static NarrativeDialogueManager& GetInstance();

    // Lua-callable API
    void StartDialogue(const std::string& name);
    void StopDialogue(const std::string& name);
    void ScrollNext(const std::string& name);
    bool IsDialogueActive(const std::string& name) const;
    bool IsAnyDialogueActive() const;
    int GetCurrentIndex(const std::string& name) const;
    // Returns autoTime of the current entry if it is Time-scroll mode, -1.0 otherwise
    float GetCurrentEntryAutoTime(const std::string& name) const;

    // Called by trigger system when a trigger is entered
    void OnTriggerEnter(Entity triggerEntity, Entity otherEntity);

    // Registration (called by DialogueSystem)
    void RegisterDialogue(const std::string& name, Entity entity);
    void UnregisterDialogue(const std::string& name);
    void Clear();

    // Get entity for a named dialogue
    Entity GetDialogueEntity(const std::string& name) const;

    // Set the ECS manager reference (called by DialogueSystem on init)
    void SetECSManager(ECSManager* ecs);

private:
    NarrativeDialogueManager() = default;
    NarrativeDialogueManager(const NarrativeDialogueManager&) = delete;
    NarrativeDialogueManager& operator=(const NarrativeDialogueManager&) = delete;

    struct DialogueRecord {
        Entity entity = INVALID_ENTITY;
    };

    std::unordered_map<std::string, DialogueRecord> m_dialogues;
    std::string m_activeDialogueName;
    ECSManager* m_ecs = nullptr;
};
