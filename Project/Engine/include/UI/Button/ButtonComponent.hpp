#pragma once
#include <string>
#include <vector>
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
};

class ButtonComponent
{
public:
    ButtonComponent() = default;  // Now truly default!
    ButtonComponent(Entity owner);
    ~ButtonComponent();

    void SetEntity(Entity owner);
    void OnEnable();
    void OnDisable();
    void OnClick();

private:
    void InstancesChangedCallback(Entity e);

private:
    Entity m_entity = 0;

    // Cache WITHOUT mutex protection - ScriptSystem handles thread-safety
    // We accept potential race conditions on cache reads since worst case is a cache miss
    std::vector<int> m_cachedInstanceRef;

    // Registration id for instance-changed callbacks
    void* m_instancesCbId = nullptr;
};