#pragma once

#include "EditorPanel.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class AudioMixer;
class AudioMixerGroup;

/**
 * @brief Audio Mixer panel for editing and managing AudioMixer assets.
 *
 * This panel provides a Unity-style interface for creating and editing audio mixer
 * hierarchies, adjusting volumes, and managing audio routing.
 */
class AudioMixerPanel : public EditorPanel {
public:
    AudioMixerPanel();
    virtual ~AudioMixerPanel() = default;

    /**
     * @brief Render the audio mixer panel's ImGui content.
     */
    void OnImGuiRender() override;

    /**
     * @brief Set the active mixer to edit.
     * @param mixer Shared pointer to the mixer to edit.
     */
    void SetActiveMixer(std::shared_ptr<AudioMixer> mixer);

    /**
     * @brief Get the currently active mixer.
     * @return Shared pointer to the active mixer, or nullptr if none.
     */
    std::shared_ptr<AudioMixer> GetActiveMixer() const { return activeMixer; }

private:
    std::shared_ptr<AudioMixer> activeMixer;
    AudioMixerGroup* selectedGroup;

    // Multiple mixer support
    std::vector<std::shared_ptr<AudioMixer>> loadedMixers;

    // UI state
    char newGroupNameBuffer[256];
    char newMixerNameBuffer[256];
    bool showCreateGroupDialog;
    bool showCreateMixerDialog;

    // Rendering helpers
    void RenderMixerSelector();
    void RenderGroupHierarchy();
    void RenderGroupHierarchyRecursive(AudioMixerGroup* group, int depth = 0);
    void RenderGroupInspector();
    void RenderMixerControls();
    void RenderMixerList();

    // Actions
    void CreateNewMixer();
    void LoadMixerFromFile();
    void SaveActiveMixer();
    void CreateNewGroup();
    void DeleteSelectedGroup();
    void DeleteMixer(std::shared_ptr<AudioMixer> mixer);

    // Helper functions
    void SelectGroup(AudioMixerGroup* group);
    bool IsGroupSelected(AudioMixerGroup* group) const;
    void LoadMixersFromResources();
};