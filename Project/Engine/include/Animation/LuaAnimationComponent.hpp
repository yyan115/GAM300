#pragma once
#include "ECS/ECSRegistry.hpp"
#include "Animation/AnimationComponent.hpp"
#include <string>

struct LuaAnimationComponent
{
    Entity entityID;

    // Constructor stores ONLY the ID, not the pointer
    LuaAnimationComponent(Entity e) : entityID(e) {}

    // Safe Lookup Helper
    AnimationComponent* GetInternal() const
    {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecs.HasComponent<AnimationComponent>(entityID))
        {
            return &ecs.GetComponent<AnimationComponent>(entityID);
        }
        return nullptr;
    }

    // --- Method Wrappers ---

    void Play()
    {
        if (auto* comp = GetInternal()) {
            comp->Play(entityID); // Pass stored ID to the component
        }
    }

    void Stop()
    {
        if (auto* comp = GetInternal()) {
            comp->Stop(entityID);
        }
    }

    void Pause()
    {
        if (auto* comp = GetInternal()) {
            comp->Pause();
        }
    }

    void PlayClip(int index, bool loop)
    {
        if (auto* comp = GetInternal()) {
            comp->PlayClip(static_cast<size_t>(index), loop, entityID);
        }
    }

    void SetSpeed(float speed)
    {
        if (auto* comp = GetInternal()) {
            comp->SetSpeed(speed);
        }
    }

    // State Machine / Animator Controller Wrappers

    void SetBool(const std::string& name, bool value)
    {
        if (auto* comp = GetInternal()) {
            comp->SetBool(name, value);
        }
    }

    void SetTrigger(const std::string& name)
    {
        if (auto* comp = GetInternal()) {
            comp->SetTrigger(name);
        }
    }

    void SetFloat(const std::string& name, float value)
    {
        if (auto* comp = GetInternal()) {
            comp->SetFloat(name, value);
        }
    }

    void SetInt(const std::string& name, int value)
    {
        if (auto* comp = GetInternal()) {
            comp->SetInt(name, value);
        }
    }

    std::string GetCurrentState() const
    {
        if (auto* comp = GetInternal()) {
            return comp->GetCurrentState();
        }
        return "";
    }

    bool IsPlaying() const
    {
        if (auto* comp = GetInternal()) {
            return comp->IsPlaying();
        }
        return false;
    }
};