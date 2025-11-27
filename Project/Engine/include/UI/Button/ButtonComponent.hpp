// ButtonBinding.hpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "Reflection/ReflectionBase.hpp"
#include "../../Utilities/GUID.hpp"
#include "Script/ScriptSystem.hpp"

struct ButtonBinding
{
    REFL_SERIALIZABLE

        std::string targetEntityGuidStr;
    std::string scriptGuidStr;     // matches ScriptData.scriptGuidStr
    std::string functionName;      // function to call, e.g. "OnPressed"
    bool callWithSelf = true;      // prefer calling as method (instance:func) - editor toggle
};

struct ButtonComponentData
{
    REFL_SERIALIZABLE
        std::vector<ButtonBinding> bindings;
    bool interactable = true;
    // You can add stuff like transition, label, etc. if desired.
};

class ButtonComponent
{
public:
    ButtonComponent(Entity owner);
    ~ButtonComponent();

    void OnEnable();   // called when component becomes active
    void OnDisable();  // called when disabled / destroyed
    void OnClick();    // call all bound functions (safe to call from main thread)

private:
    void InvalidateCacheForEntity(Entity e);
    void InstancesChangedCallback(Entity e);

private:
    Entity m_entity;
    //ScriptSystem* m_scriptSystem; *USE ECSRegistry::GetInstance().GetActiveECSManager().scriptSystem->GetInstanceRefForScript(...)*

    // cache: binding index -> instanceRef (LUA registry ref). We cache to avoid repeated map lookups.
    // We store LUA_NOREF if unknown.
    std::vector<int> m_cachedInstanceRef;

    // simple lock to protect cache in multithreaded access (though calls to Lua should be on main thread).
    std::mutex m_cacheMutex;

    // registration id for instance-changed callbacks (we use pointer of lambda target as id, see ScriptSystem)
    void* m_instancesCbId = nullptr;
};