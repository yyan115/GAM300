#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include "AudioMixerGroup.hpp"
#include "Reflection/ReflectionBase.hpp"
#include "../Engine.h"

// Forward declarations
typedef struct FMOD_SYSTEM FMOD_SYSTEM;
typedef struct FMOD_CHANNELGROUP FMOD_CHANNELGROUP;

/**
 * @brief Represents an audio mixer asset following Unity's standard.
 * 
 * An AudioMixer manages a hierarchy of AudioMixerGroups (buses) for advanced audio mixing.
 * It provides centralized control over audio routing, volume hierarchies, and effects.
 * Unlike Unity's ScriptableObject, this is a simple serializable class that doesn't
 * go through the asset compilation pipeline.
 */
class ENGINE_API AudioMixer {
public:
    REFL_SERIALIZABLE

    AudioMixer();
    virtual ~AudioMixer();

    // Core properties
    const std::string& GetName() const;
    void SetName(const std::string& newName);

    // Group management
    AudioMixerGroup* CreateGroup(const std::string& groupName);
    AudioMixerGroup* CreateGroup(const std::string& groupName, AudioMixerGroup* parentGroup);
    bool RemoveGroup(const std::string& groupName);
    AudioMixerGroup* GetGroup(const std::string& groupName) const;
    AudioMixerGroup* GetGroupByPath(const std::string& groupPath) const;
    
    // Get all groups
    const std::unordered_map<std::string, std::shared_ptr<AudioMixerGroup>>& GetAllGroups() const;
    
    // Master group (root of hierarchy)
    AudioMixerGroup* GetMasterGroup() const;

    // FMOD system access (internal use)
    FMOD_SYSTEM* GetFMODSystem() const;

    // Serialization using reflection
    bool SaveToFile(const std::string& filePath);
    bool LoadFromFile(const std::string& filePath);

    // Apply mixer configuration to AudioManager
    void ApplyToAudioManager();

    // Access groups for serialization
    std::vector<AudioMixerGroup*> GetGroupsList() const;

private:
    std::string MixerName;
    
    // Group hierarchy
    std::shared_ptr<AudioMixerGroup> MasterGroup;
    std::unordered_map<std::string, std::shared_ptr<AudioMixerGroup>> Groups;
    
    // Thread safety
    mutable std::mutex Mutex;

    // Helper functions
    void InitializeMasterGroup();
    void DestroyAllGroups();
    AudioMixerGroup* FindGroupRecursive(AudioMixerGroup* current, const std::string& path) const;
    void WriteJSONToStream(std::ostream& ofs);
};
