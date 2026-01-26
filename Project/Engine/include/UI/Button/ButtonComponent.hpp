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
    std::string scriptPath;
    std::string scriptGuidStr;     // matches ScriptData.scriptGuidStr -> Supposed to use this to save on memory & data protection
    std::string functionName;      // function to call, e.g. "OnClick"
    bool callWithSelf = true;      // prefer calling as method (instance:func) - editor toggle
};

struct ButtonComponent
{
    REFL_SERIALIZABLE
        std::vector<ButtonBinding> bindings;
    bool interactable = true;
};

class ButtonController
{
public:
    ButtonController() = default;  // Now truly default!
    ButtonController(Entity owner);
    ~ButtonController();

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