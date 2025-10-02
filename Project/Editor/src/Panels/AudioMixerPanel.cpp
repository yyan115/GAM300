#include "Panels/AudioMixerPanel.hpp"
#include "Sound/AudioMixer.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "imgui.h"
#include <algorithm>
#include <filesystem>

AudioMixerPanel::AudioMixerPanel()
    : EditorPanel("Audio Mixer", false)
    , activeMixer(nullptr)
    , selectedGroup(nullptr)
    , showCreateGroupDialog(false)
    , showCreateMixerDialog(false)
{
    memset(newGroupNameBuffer, 0, sizeof(newGroupNameBuffer));
    memset(newMixerNameBuffer, 0, sizeof(newMixerNameBuffer));
    
    // Load existing mixers from Resources/Audio
    LoadMixersFromResources();
}

void AudioMixerPanel::OnImGuiRender() {
    if (!ImGui::Begin(name.c_str(), &isOpen)) {
        ImGui::End();
        return;
    }

    // Top toolbar
    RenderMixerControls();
    
    ImGui::Separator();
    
    // Unity-style three-column layout: Mixer List | Group Hierarchy | Inspector
    float mixerListWidth = 200.0f;
    float hierarchyWidth = 300.0f;
    
    // Mixer List (left column)
    ImGui::BeginChild("MixerList", ImVec2(mixerListWidth, 0), true);
    RenderMixerList();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    if (!activeMixer) {
        ImGui::BeginChild("NoMixer", ImVec2(0, 0), true);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No mixer selected.");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Create or select a mixer to begin.");
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    
    // Group Hierarchy (middle column)
    ImGui::BeginChild("MixerHierarchy", ImVec2(hierarchyWidth, 0), true);
    RenderGroupHierarchy();
    ImGui::EndChild();

    ImGui::SameLine();

    // Inspector (right column)
    ImGui::BeginChild("GroupInspector", ImVec2(0, 0), true);
    RenderGroupInspector();
    ImGui::EndChild();

    // Create group dialog
    if (showCreateGroupDialog) {
        ImGui::OpenPopup("Create Audio Mixer Group");
        showCreateGroupDialog = false;
    }

    // Create mixer dialog
    if (showCreateMixerDialog) {
        ImGui::OpenPopup("Create Audio Mixer");
        showCreateMixerDialog = false;
    }

    // Group creation popup
    if (ImGui::BeginPopupModal("Create Audio Mixer Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter group name:");
        ImGui::InputText("##GroupName", newGroupNameBuffer, sizeof(newGroupNameBuffer));

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            CreateNewGroup();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    
    // Mixer creation popup
    if (ImGui::BeginPopupModal("Create Audio Mixer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter mixer name:");
        ImGui::InputText("##MixerName", newMixerNameBuffer, sizeof(newMixerNameBuffer));

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (strlen(newMixerNameBuffer) > 0) {
                auto newMixer = std::make_shared<AudioMixer>();
                newMixer->SetName(newMixerNameBuffer);
                loadedMixers.push_back(newMixer);
                SetActiveMixer(newMixer);
                memset(newMixerNameBuffer, 0, sizeof(newMixerNameBuffer));
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

void AudioMixerPanel::SetActiveMixer(std::shared_ptr<AudioMixer> mixer) {
    activeMixer = mixer;
    selectedGroup = mixer ? mixer->GetMasterGroup() : nullptr;
}

void AudioMixerPanel::RenderMixerControls() {
    if (ImGui::Button("New Mixer")) {
        showCreateMixerDialog = true;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Load Mixer")) {
        LoadMixerFromFile();
    }
    ImGui::SameLine();
    
    if (activeMixer && ImGui::Button("Save Mixer")) {
        SaveActiveMixer();
    }
    ImGui::SameLine();
    
    if (activeMixer && ImGui::Button("Apply to AudioManager")) {
        activeMixer->ApplyToAudioManager();
    }

    if (activeMixer) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Mixer: %s", activeMixer->GetName().c_str());
    }
}

void AudioMixerPanel::RenderGroupHierarchy() {
    ImGui::Text("Mixer Groups");
    ImGui::Separator();

    if (ImGui::Button("Add Group", ImVec2(-1, 0))) {
        showCreateGroupDialog = true;
    }

    ImGui::Separator();

    if (activeMixer) {
        AudioMixerGroup* masterGroup = activeMixer->GetMasterGroup();
        if (masterGroup) {
            RenderGroupHierarchyRecursive(masterGroup, 0);
        }
    }
}

void AudioMixerPanel::RenderGroupHierarchyRecursive(AudioMixerGroup* group, int depth) {
    if (!group) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    if (IsGroupSelected(group)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (group->GetChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Indent based on depth
    if (depth > 0) {
        ImGui::Indent(16.0f);
    }

    // Volume indicator color
    float volume = group->GetVolume();
    ImVec4 volumeColor = group->IsMuted() ? ImVec4(0.5f, 0.5f, 0.5f, 1.0f) : 
                         volume > 0.8f ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) :
                         volume > 0.3f ? ImVec4(1.0f, 1.0f, 0.5f, 1.0f) :
                         ImVec4(1.0f, 0.5f, 0.5f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, volumeColor);
    
    bool nodeOpen = false;
    if (group->GetChildren().empty()) {
        ImGui::TreeNodeEx(group, flags, "%s [%.2f]", group->GetName().c_str(), volume);
    } else {
        nodeOpen = ImGui::TreeNodeEx(group, flags, "%s [%.2f]", group->GetName().c_str(), volume);
    }
    
    ImGui::PopStyleColor();

    // Handle selection
    if (ImGui::IsItemClicked()) {
        SelectGroup(group);
    }
    
    // Drag-drop source for AudioMixerGroup routing
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        std::string groupPath = group->GetFullPath();
        ImGui::SetDragDropPayload("AUDIOMIXERGROUP_DRAG", groupPath.c_str(), groupPath.size() + 1);
        ImGui::Text("AudioMixerGroup: %s", groupPath.c_str());
        ImGui::EndDragDropSource();
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Add Child Group")) {
            showCreateGroupDialog = true;
            SelectGroup(group);
        }
        
        if (group->GetName() != "Master" && ImGui::MenuItem("Delete Group")) {
            if (activeMixer) {
                activeMixer->RemoveGroup(group->GetName());
                if (selectedGroup == group) {
                    selectedGroup = activeMixer->GetMasterGroup();
                }
            }
        }
        
        ImGui::EndPopup();
    }

    // Render children
    if (nodeOpen && !group->GetChildren().empty()) {
        for (AudioMixerGroup* child : group->GetChildren()) {
            RenderGroupHierarchyRecursive(child, depth + 1);
        }
        ImGui::TreePop();
    }

    if (depth > 0) {
        ImGui::Unindent(16.0f);
    }
}

void AudioMixerPanel::RenderGroupInspector() {
    if (!selectedGroup) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a group to edit its properties.");
        return;
    }

    ImGui::Text("Group: %s", selectedGroup->GetName().c_str());
    ImGui::Text("Path: %s", selectedGroup->GetFullPath().c_str());
    ImGui::Separator();

    // Volume control
    float volume = selectedGroup->GetVolume();
    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f")) {
        selectedGroup->SetVolume(volume);
    }

    // Volume dB display
    float volumeDb = volume > 0.0f ? 20.0f * log10f(volume) : -80.0f;
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%.1f dB", volumeDb);

    // Pitch control
    float pitch = selectedGroup->GetPitch();
    if (ImGui::SliderFloat("Pitch", &pitch, 0.5f, 2.0f, "%.2f")) {
        selectedGroup->SetPitch(pitch);
    }

    ImGui::Separator();

    // Mute/Solo/Pause controls
    bool isMuted = selectedGroup->IsMuted();
    if (ImGui::Checkbox("Mute", &isMuted)) {
        selectedGroup->SetMuted(isMuted);
    }

    ImGui::SameLine();
    bool isSolo = selectedGroup->IsSolo();
    if (ImGui::Checkbox("Solo", &isSolo)) {
        selectedGroup->SetSolo(isSolo);
    }

    ImGui::SameLine();
    bool isPaused = selectedGroup->IsPaused();
    if (ImGui::Checkbox("Pause", &isPaused)) {
        selectedGroup->SetPaused(isPaused);
    }

    ImGui::Separator();

    // Parent group selection
    ImGui::Text("Parent Group:");
    if (selectedGroup->GetParent()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", selectedGroup->GetParent()->GetName().c_str());
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(Root)");
    }

    // Children info
    ImGui::Text("Children: %zu", selectedGroup->GetChildren().size());

    ImGui::Separator();

    // Quick actions
    if (selectedGroup->GetName() != "Master") {
        if (ImGui::Button("Delete Group", ImVec2(-1, 0))) {
            DeleteSelectedGroup();
        }
    }
}

void AudioMixerPanel::CreateNewMixer() {
    activeMixer = std::make_shared<AudioMixer>();
    activeMixer->SetName("New Audio Mixer");
    selectedGroup = activeMixer->GetMasterGroup();
}

void AudioMixerPanel::LoadMixerFromFile() {
    // In a real implementation, this would open a file dialog
    // For now, we'll just create a new mixer
    // TODO: Implement file dialog integration
    CreateNewMixer();
}

void AudioMixerPanel::SaveActiveMixer() {
    if (!activeMixer) return;

    // In a real implementation, this would open a save dialog
    // For now, save to a default location
    std::string savePath = "Resources/Audio/" + activeMixer->GetName() + ".mixer";
    
    // Ensure directory exists
    std::filesystem::create_directories("Resources/Audio");
    
    if (activeMixer->SaveToFile(savePath)) {
        // Log success
        ImGui::OpenPopup("Save Success");
    } else {
        ImGui::OpenPopup("Save Failed");
    }
}

void AudioMixerPanel::CreateNewGroup() {
    if (!activeMixer || strlen(newGroupNameBuffer) == 0) {
        return;
    }

    std::string groupName(newGroupNameBuffer);
    AudioMixerGroup* parent = selectedGroup ? selectedGroup : activeMixer->GetMasterGroup();
    
    AudioMixerGroup* newGroup = activeMixer->CreateGroup(groupName, parent);
    if (newGroup) {
        SelectGroup(newGroup);
    }

    // Clear buffer
    memset(newGroupNameBuffer, 0, sizeof(newGroupNameBuffer));
}

void AudioMixerPanel::DeleteSelectedGroup() {
    if (!activeMixer || !selectedGroup || selectedGroup->GetName() == "Master") {
        return;
    }

    std::string groupName = selectedGroup->GetName();
    AudioMixerGroup* parent = selectedGroup->GetParent();
    
    if (activeMixer->RemoveGroup(groupName)) {
        selectedGroup = parent ? parent : activeMixer->GetMasterGroup();
    }
}

void AudioMixerPanel::DeleteMixer(std::shared_ptr<AudioMixer> mixer) {
    if (!mixer) return;
    
    // Remove from loaded mixers
    auto it = std::find(loadedMixers.begin(), loadedMixers.end(), mixer);
    if (it != loadedMixers.end()) {
        loadedMixers.erase(it);
    }
    
    // If this was the active mixer, clear it
    if (activeMixer == mixer) {
        activeMixer = nullptr;
        selectedGroup = nullptr;
    }
}

void AudioMixerPanel::RenderMixerList() {
    ImGui::Text("Audio Mixers");
    ImGui::Separator();
    
    if (ImGui::Button("+ New Mixer", ImVec2(-1, 0))) {
        showCreateMixerDialog = true;
    }
    
    ImGui::Separator();
    
    // List all loaded mixers
    for (auto& mixer : loadedMixers) {
        bool isActive = (mixer == activeMixer);
        
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isActive) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        
        ImGui::TreeNodeEx(mixer.get(), flags, "%s", mixer->GetName().c_str());
        
        // Click to select
        if (ImGui::IsItemClicked()) {
            SetActiveMixer(mixer);
        }
        
        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                DeleteMixer(mixer);
                ImGui::EndPopup();
                break; // Exit loop since we modified the container
            }
            ImGui::EndPopup();
        }
    }
}

void AudioMixerPanel::LoadMixersFromResources() {
    std::string audioPath = "Resources/Audio";
    
    // Check if directory exists
    if (!std::filesystem::exists(audioPath)) {
        std::filesystem::create_directories(audioPath);
        return;
    }
    
    // Scan for .mixer files
    for (const auto& entry : std::filesystem::directory_iterator(audioPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".mixer") {
            // Skip Android-specific files
            std::string filename = entry.path().filename().string();
            if (filename.find("_android") != std::string::npos) {
                continue;
            }
            
            // Load the mixer
            auto mixer = std::make_shared<AudioMixer>();
            if (mixer->LoadFromFile(entry.path().string())) {
                loadedMixers.push_back(mixer);
            }
        }
    }
}

void AudioMixerPanel::SelectGroup(AudioMixerGroup* group) {
    selectedGroup = group;
}

bool AudioMixerPanel::IsGroupSelected(AudioMixerGroup* group) const {
    return selectedGroup == group;
}
