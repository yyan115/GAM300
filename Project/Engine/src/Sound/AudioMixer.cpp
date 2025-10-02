#include "pch.h"
#include "Sound/AudioMixer.hpp"
#include "Sound/AudioManager.hpp"
#include <fmod.h>
#include <fmod_errors.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "Logging.hpp"

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/log.h>
#include "WindowManager.hpp"
#include "Platform/AndroidPlatform.h"
#endif

#include "Utilities/FileUtilities.hpp"
#include "Asset Manager/AssetManager.hpp"

#pragma region Reflection
REFL_REGISTER_START(AudioMixer)
    REFL_REGISTER_PROPERTY(MixerName)
REFL_REGISTER_END
#pragma endregion

AudioMixer::AudioMixer()
    : MixerName("New Mixer")
    , MasterGroup(nullptr)
{
    InitializeMasterGroup();
}

AudioMixer::~AudioMixer() {
    DestroyAllGroups();
}

void AudioMixer::InitializeMasterGroup() {
    std::lock_guard<std::mutex> lock(Mutex);
    
    // Create master group (root of hierarchy)
    MasterGroup = std::make_shared<AudioMixerGroup>("Master", this);
    Groups["Master"] = MasterGroup;
}

void AudioMixer::DestroyAllGroups() {
    std::lock_guard<std::mutex> lock(Mutex);
    
    Groups.clear();
    MasterGroup.reset();
}

const std::string& AudioMixer::GetName() const {
    return MixerName;
}

void AudioMixer::SetName(const std::string& newName) {
    MixerName = newName;
}

const std::unordered_map<std::string, std::shared_ptr<AudioMixerGroup>>& AudioMixer::GetAllGroups() const {
    return Groups;
}

AudioMixerGroup* AudioMixer::GetMasterGroup() const {
    return MasterGroup.get();
}

FMOD_SYSTEM* AudioMixer::GetFMODSystem() const {
    // Access the AudioManager's FMOD system
    return AudioManager::GetInstance().GetFMODSystem();
}

AudioMixerGroup* AudioMixer::CreateGroup(const std::string& groupName) {
    return CreateGroup(groupName, MasterGroup.get());
}

AudioMixerGroup* AudioMixer::CreateGroup(const std::string& groupName, AudioMixerGroup* parentGroup) {
    std::lock_guard<std::mutex> lock(Mutex);
    
    if (!parentGroup) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, 
            "[AudioMixer] Cannot create group without parent\n");
        return nullptr;
    }

    // Check if group already exists
    if (Groups.find(groupName) != Groups.end()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, 
            "[AudioMixer] Group already exists: ", groupName, "\n");
        return Groups[groupName].get();
    }

    // Create new group
    auto newGroup = std::make_shared<AudioMixerGroup>(groupName, this);
    newGroup->SetParent(parentGroup);
    
    Groups[groupName] = newGroup;
    
    ENGINE_PRINT("[AudioMixer] Created group: ", groupName, " under ", parentGroup->GetName(), "\n");
    return newGroup.get();
}

bool AudioMixer::RemoveGroup(const std::string& groupName) {
    std::lock_guard<std::mutex> lock(Mutex);
    
    // Cannot remove master group
    if (groupName == "Master") {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, 
            "[AudioMixer] Cannot remove Master group\n");
        return false;
    }

    auto it = Groups.find(groupName);
    if (it == Groups.end()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, 
            "[AudioMixer] Group not found: ", groupName, "\n");
        return false;
    }

    // Remove from parent
    AudioMixerGroup* group = it->second.get();
    if (group->GetParent()) {
        group->GetParent()->RemoveChild(group);
    }

    // Re-parent children to this group's parent
    AudioMixerGroup* parent = group->GetParent();
    for (AudioMixerGroup* child : group->GetChildren()) {
        child->SetParent(parent);
    }

    // Remove from map
    Groups.erase(it);
    
    ENGINE_PRINT("[AudioMixer] Removed group: ", groupName, "\n");
    return true;
}

AudioMixerGroup* AudioMixer::GetGroup(const std::string& groupName) const {
    std::lock_guard<std::mutex> lock(Mutex);
    
    auto it = Groups.find(groupName);
    if (it != Groups.end()) {
        return it->second.get();
    }
    return nullptr;
}

AudioMixerGroup* AudioMixer::GetGroupByPath(const std::string& groupPath) const {
    std::lock_guard<std::mutex> lock(Mutex);
    
    return FindGroupRecursive(MasterGroup.get(), groupPath);
}

AudioMixerGroup* AudioMixer::FindGroupRecursive(AudioMixerGroup* current, const std::string& path) const {
    if (!current) return nullptr;
    
    if (current->GetFullPath() == path) {
        return current;
    }
    
    for (AudioMixerGroup* child : current->GetChildren()) {
        AudioMixerGroup* found = FindGroupRecursive(child, path);
        if (found) return found;
    }
    
    return nullptr;
}

void AudioMixer::ApplyToAudioManager() {
    std::lock_guard<std::mutex> lock(Mutex);
    
    AudioManager& audioManager = AudioManager::GetInstance();
    
    // Apply each group's settings to corresponding buses in AudioManager
    for (const auto& pair : Groups) {
        const std::string& groupName = pair.first;
        AudioMixerGroup* group = pair.second.get();
        
        // Get or create corresponding bus
        FMOD_CHANNELGROUP* bus = audioManager.GetOrCreateBus(groupName);
        
        // Apply group settings to bus
        if (bus) {
            group->SetFMODChannelGroup(bus);
            audioManager.SetBusVolume(groupName, group->GetVolume());
            audioManager.SetBusPaused(groupName, group->IsPaused());
        }
    }
    
    ENGINE_PRINT("[AudioMixer] Applied mixer configuration to AudioManager\n");
}

std::vector<AudioMixerGroup*> AudioMixer::GetGroupsList() const {
    std::lock_guard<std::mutex> lock(Mutex);
    
    std::vector<AudioMixerGroup*> groupsList;
    for (const auto& pair : Groups) {
        groupsList.push_back(pair.second.get());
    }
    return groupsList;
}
// Simple JSON serialization without rapidjson dependency
// Uses reflection system for type information but manual JSON writing
// Supports saving for Android builds
bool AudioMixer::SaveToFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(Mutex);
    
    try {
        // Determine output path
        std::string outPath = filePath;
        
#ifdef EDITOR
        // For Android builds, save to Android resources directory
        std::filesystem::path p(filePath);
        bool needsAndroidCopy = p.extension() == ".mixer";
        
        if (needsAndroidCopy) {
            // Create Android version in resources folder
            std::filesystem::path androidPath = AssetManager::GetInstance().GetAndroidResourcesPath() / 
                                                p.parent_path() / 
                                                (p.stem().string() + "_android.mixer");
            
            // Ensure parent directories exist
            std::filesystem::create_directories(androidPath.parent_path());
            
            // Save Android version
            std::ofstream androidOfs(androidPath);
            if (androidOfs.is_open()) {
                WriteJSONToStream(androidOfs);
                androidOfs.close();
                ENGINE_PRINT("[AudioMixer] Saved Android mixer to: ", androidPath.string(), "\n");
            }
        }
        
        // Also save to project root for editor use
        outPath = (FileUtilities::GetSolutionRootDir() / filePath).string();
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
#endif
        
        std::ofstream ofs(outPath);
        if (!ofs.is_open()) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, 
                "[AudioMixer] Failed to open file for writing: ", outPath, "\n");
            return false;
        }
        
        WriteJSONToStream(ofs);
        ofs.close();
        
        ENGINE_PRINT("[AudioMixer] Saved mixer to: ", outPath, "\n");
        return true;
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, 
            "[AudioMixer] Exception during save: ", e.what(), "\n");
        return false;
    }
}

// Helper method to write JSON content to a stream
void AudioMixer::WriteJSONToStream(std::ostream& ofs) {
    // Write JSON manually (Unity-style format)
    ofs << "{\n";
    ofs << "  \"name\": \"" << MixerName << "\",\n";
    ofs << "  \"groups\": [\n";
    
    // Serialize master group first
    bool first = true;
    auto serializeGroup = [&](AudioMixerGroup* group) {
        if (!first) ofs << ",\n";
        first = false;
        
        ofs << "    {\n";
        ofs << "      \"name\": \"" << group->GetName() << "\",\n";
        ofs << "      \"volume\": " << group->GetVolume() << ",\n";
        ofs << "      \"pitch\": " << group->GetPitch() << ",\n";
        ofs << "      \"muted\": " << (group->IsMuted() ? "true" : "false") << ",\n";
        ofs << "      \"solo\": " << (group->IsSolo() ? "true" : "false") << ",\n";
        ofs << "      \"paused\": " << (group->IsPaused() ? "true" : "false");
        
        if (group->GetParent()) {
            ofs << ",\n";
            ofs << "      \"parent\": \"" << group->GetParent()->GetName() << "\"\n";
        } else {
            ofs << "\n";
        }
        
        ofs << "    }";
    };
    
    // Serialize master group
    serializeGroup(MasterGroup.get());
    
    // Serialize all other groups
    for (const auto& [name, group] : Groups) {
        if (name != "Master") {
            serializeGroup(group.get());
        }
    }
    
    ofs << "\n  ]\n";
    ofs << "}\n";
}

// Simple JSON deserialization without rapidjson dependency
// Parses basic JSON structure manually
// Supports loading from Android assets and regular filesystem
bool AudioMixer::LoadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(Mutex);
    
    try {
        std::string content;
        
#ifdef __ANDROID__
        // On Android, try loading from AssetManager first
        auto* platform = WindowManager::GetPlatform();
        if (platform) {
            AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
            AAssetManager* assetManager = androidPlatform->GetAssetManager();

            if (assetManager) {
                AAsset* asset = AAssetManager_open(assetManager, filePath.c_str(), AASSET_MODE_BUFFER);
                if (asset) {
                    off_t assetLength = AAsset_getLength(asset);
                    const char* assetData = (const char*)AAsset_getBuffer(asset);

                    if (assetData && assetLength > 0) {
                        content = std::string(assetData, assetLength);
                        __android_log_print(ANDROID_LOG_INFO, "GAM300", 
                            "[AudioMixer] Loaded mixer from Android assets: %s", filePath.c_str());
                    } else {
                        __android_log_print(ANDROID_LOG_ERROR, "GAM300", 
                            "[AudioMixer] Failed to get asset data for: %s", filePath.c_str());
                    }
                    AAsset_close(asset);
                } else {
                    __android_log_print(ANDROID_LOG_WARN, "GAM300", 
                        "[AudioMixer] Asset not found in AssetManager: %s, trying filesystem", filePath.c_str());
                }
            }
        }
        
        // If not loaded from assets, try filesystem
        if (content.empty()) {
            std::ifstream ifs(filePath);
            if (ifs.is_open()) {
                std::stringstream buffer;
                buffer << ifs.rdbuf();
                content = buffer.str();
                ifs.close();
            } else {
                __android_log_print(ANDROID_LOG_ERROR, "GAM300", 
                    "[AudioMixer] Failed to open file: %s", filePath.c_str());
                return false;
            }
        }
#else
        // On other platforms, load from filesystem directly
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, 
                "[AudioMixer] Failed to open file for reading: ", filePath, "\n");
            return false;
        }
        
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        content = buffer.str();
        ifs.close();
#endif
        
        // Clear existing groups
        DestroyAllGroups();
        InitializeMasterGroup();
        
        // Simple JSON parsing (looking for key patterns)
        auto findValue = [](const std::string& str, const std::string& key, size_t startPos = 0) -> std::string {
            std::string searchKey = "\"" + key + "\":";
            size_t pos = str.find(searchKey, startPos);
            if (pos == std::string::npos) return "";
            
            pos += searchKey.length();
            while (pos < str.length() && (str[pos] == ' ' || str[pos] == '\t')) pos++;
            
            if (str[pos] == '\"') {
                // String value
                size_t start = ++pos;
                size_t end = str.find('\"', start);
                if (end != std::string::npos) {
                    return str.substr(start, end - start);
                }
            } else if (str[pos] == 't' && str.substr(pos, 4) == "true") {
                return "true";
            } else if (str[pos] == 'f' && str.substr(pos, 5) == "false") {
                return "false";
            } else {
                // Numeric value
                size_t start = pos;
                size_t end = str.find_first_of(",}\n", start);
                if (end != std::string::npos) {
                    return str.substr(start, end - start);
                }
            }
            return "";
        };
        
        // Extract mixer name
        std::string name = findValue(content, "name");
        if (!name.empty()) {
            MixerName = name;
        }
        
        // Extract groups array
        size_t groupsStart = content.find("\"groups\":");
        if (groupsStart != std::string::npos) {
            size_t arrayStart = content.find('[', groupsStart);
            size_t arrayEnd = content.rfind(']');
            
            if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                std::string groupsContent = content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                
                // Parse each group object
                size_t objStart = 0;
                while ((objStart = groupsContent.find('{', objStart)) != std::string::npos) {
                    size_t objEnd = groupsContent.find('}', objStart);
                    if (objEnd == std::string::npos) break;
                    
                    std::string groupObj = groupsContent.substr(objStart, objEnd - objStart + 1);
                    
                    // Parse group properties
                    std::string groupName = findValue(groupObj, "name");
                    std::string volumeStr = findValue(groupObj, "volume");
                    std::string pitchStr = findValue(groupObj, "pitch");
                    std::string mutedStr = findValue(groupObj, "muted");
                    std::string soloStr = findValue(groupObj, "solo");
                    std::string pausedStr = findValue(groupObj, "paused");
                    std::string parentName = findValue(groupObj, "parent");
                    
                    if (!groupName.empty()) {
                        AudioMixerGroup* group = nullptr;
                        
                        if (groupName == "Master") {
                            group = MasterGroup.get();
                        } else {
                            auto newGroup = std::make_shared<AudioMixerGroup>(groupName, this);
                            Groups[groupName] = newGroup;
                            group = newGroup.get();
                        }
                        
                        // Set properties
                        if (!volumeStr.empty()) {
                            group->SetVolume(std::stof(volumeStr));
                        }
                        if (!pitchStr.empty()) {
                            group->SetPitch(std::stof(pitchStr));
                        }
                        if (!mutedStr.empty()) {
                            group->SetMuted(mutedStr == "true");
                        }
                        if (!soloStr.empty()) {
                            group->SetSolo(soloStr == "true");
                        }
                        if (!pausedStr.empty()) {
                            group->SetPaused(pausedStr == "true");
                        }
                    }
                    
                    objStart = objEnd + 1;
                }
                
                // Second pass: set up hierarchy
                objStart = 0;
                while ((objStart = groupsContent.find('{', objStart)) != std::string::npos) {
                    size_t objEnd = groupsContent.find('}', objStart);
                    if (objEnd == std::string::npos) break;
                    
                    std::string groupObj = groupsContent.substr(objStart, objEnd - objStart + 1);
                    
                    std::string groupName = findValue(groupObj, "name");
                    std::string parentName = findValue(groupObj, "parent");
                    
                    if (!groupName.empty() && groupName != "Master") {
                        AudioMixerGroup* group = GetGroup(groupName);
                        if (group) {
                            if (!parentName.empty()) {
                                AudioMixerGroup* parent = GetGroup(parentName);
                                if (parent) {
                                    group->SetParent(parent);
                                }
                            } else {
                                group->SetParent(MasterGroup.get());
                            }
                        }
                    }
                    
                    objStart = objEnd + 1;
                }
            }
        }
        
        ENGINE_PRINT("[AudioMixer] Loaded mixer from: ", filePath, "\n");
        return true;
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, 
            "[AudioMixer] Exception during load: ", e.what(), "\n");
        return false;
    }
}
