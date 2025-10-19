#pragma once

#include "Entity.hpp"
#include <set>
#include <string>
#include <typeinfo>

class System {
public:
    std::set<Entity> entities; // Set of entities that are part of this system.
    
    // Get the display name of this system (auto-extracted from class name)
    virtual std::string GetSystemName() const {
        // Extract class name from typeid, removing "class " prefix if present
        std::string fullName = typeid(*this).name();
        
        // On MSVC, typeid().name() returns "class ClassName"
        // On GCC/Clang, it returns mangled names
        size_t classPos = fullName.find("class ");
        if (classPos != std::string::npos) {
            fullName = fullName.substr(classPos + 6);
        }
        
        // Remove "System" suffix if present for cleaner names
        size_t systemPos = fullName.find("System");
        if (systemPos != std::string::npos && systemPos == fullName.length() - 6) {
            fullName = fullName.substr(0, systemPos);
        }
        
        return fullName;
    }
    
    virtual ~System() = default;
};