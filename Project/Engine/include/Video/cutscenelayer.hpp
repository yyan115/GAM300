#pragma once

#include "pch.h"
#include <sstream>
#include<algorithm>
#include <fstream>
#include <string>
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Transform/TransformComponent.hpp"

class DialogueManager
{
    // --- Data Containers ---
    // Replacing StringIndex with standard strings
    std::vector<std::vector<std::string>> m_CutsceneDialogue;

    // Index management
    size_t m_currSectionIndex = 0;   // Which cutscene section is active
    size_t m_currDialogueIndex = 0;  // Which dialogue line is active

    // Timing and state management
    float m_dialogueTimer = 0.0f;     // Accumulates time for text animation
    float m_dialogueTextRate = 50.0f; // Characters per second (0.02s per character)

    //float m_dialogueHoldTimer = 0.0f; // Time after text is fully revealed
    //float m_dialogueHoldDuration = 1.0f;

    bool is_autoplay = false;
    bool m_dialogueIsWaitingForInput = false;

    // Visual feedback
    std::string m_currentVisibleText; // The text currently appearing on UI
    int lastFrame = -1;

    // Progressive text tracking (for board-based text within panels)
    int m_lastPanel = -1;
    size_t m_previousBoardTextLength = 0;  // How many chars were shown on previous board
    std::string m_previousBoardText;       // Text from previous board for comparison

public:
    DialogueManager() = default;
    ~DialogueManager() = default;

    std::unordered_map<int, std::string> dialogueMap;       // Frame-based dialogue (legacy)
    std::unordered_map<int, std::string> panelDialogueMap;  // Panel-based dialogue (new)
    float m_timer = 0;



//    // --- Core Integration Functions ---

//    // Loads your dialogue data from your new engine's parser
//    void loadCutscene(const std::vector<std::vector<std::string>>& dialogueData);

//    // Call this in your new engine's main Update loop
//    void Update(float dt);

//    // --- Original Function Names ---

//    // Updates the animated display of characters (Typewriter effect)
//    void updateDialogueText(float dt);

//    // Advances to the next line or section
//    void advanceDialogue();

//    // Handles automatic progression logic
//    void updateDialogueAuto(float dt);

//    // Handles manual input-based progression
//    void updateDialogueManual(float dt);

//    // --- Getters for your New Engine Renderer ---
//    const std::string& GetCurrentText() const { return m_currentVisibleText; }
//    bool IsWaitingForInput() const { return m_dialogueIsWaitingForInput; }

    //GETTER FUNCTIONS FOR MAP
    //std::unordered_map GetDialogueMap()     {return dialogueMap;}

    void HandleTextLogic();


    //current frame -> go to map and find out where
    void HandleTextRender(float deltaTime, TextRenderComponent& textComp, Transform& textTransform, int currentFrame, bool instantRender = false);

    // Panel-based text rendering (new system)
    void HandlePanelTextRender(float deltaTime, TextRenderComponent& textComp, Transform& textTransform, int currentPanel, bool instantRender = false);

    //Reset the string rendered
    void Reset();

    bool IsTextFinished(TextRenderComponent& textComp, int currentFrame);

    // Panel-based text completion check
    bool IsTextFinishedForPanel(TextRenderComponent& textComp, int currentPanel);

    // Complete text immediately (for tap-to-skip typewriter)
    void CompleteTextImmediately(TextRenderComponent& textComp, int currentPanel);

    void AdvanceDialogue(float deltaTime, TextRenderComponent& textComp, Transform& textTransform, int currentFrame);

};
