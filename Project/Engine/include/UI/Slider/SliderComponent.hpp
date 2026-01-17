#pragma once
#include <string>
#include <vector>
#include "Reflection/ReflectionBase.hpp"
#include "../../Utilities/GUID.hpp"

struct SliderBinding
{
    REFL_SERIALIZABLE
        std::string targetEntityGuidStr;
    std::string scriptPath;
    std::string scriptGuidStr;
    std::string functionName;      // function to call, e.g. "OnValueChanged"
    bool callWithSelf = true;      // prefer calling as method (instance:func) - editor toggle
};

struct SliderComponent
{
    REFL_SERIALIZABLE
        std::vector<SliderBinding> onValueChanged;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float value = 0.0f;
    bool wholeNumbers = false;
    bool interactable = true;
    bool horizontal = true; // true = horizontal, false = vertical
    GUID_128 trackEntityGuid{};
    GUID_128 handleEntityGuid{};

    // Runtime-only cache
    float lastValue = 0.0f;
};
