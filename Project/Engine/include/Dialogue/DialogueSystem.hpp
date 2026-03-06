#pragma once
#include "ECS/System.hpp"

class ECSManager;

class DialogueSystem : public System {
public:
    DialogueSystem() = default;
    ~DialogueSystem() = default;

    void Initialise(ECSManager& ecsManager);
    void Update(float dt);
    void Shutdown();

private:
    void AdvanceToNextEntry(struct DialogueComponent& dialogue);
    void BeginEntry(struct DialogueComponent& dialogue);
    void EndDialogue(struct DialogueComponent& dialogue);

    ECSManager* m_ecs = nullptr;
};
