#include "pch.h"
#include "Video/cutscenelayer.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/TagComponent.hpp"
#include "Transform/TransformComponent.hpp"


void DialogueManager::HandleTextRender(float deltaTime, TextRenderComponent& textComp, Transform& textTransform, int currentFrame, bool instantRender)
{
    auto it = dialogueMap.find(currentFrame);
    if (it == dialogueMap.end())
    {
        textComp.text = "";
        return;
    }
    std::string fullText = it->second;

    if (currentFrame != lastFrame)
    {
        // Check if we should continue typewriter from previous board
        bool shouldContinue = false;

        if (!m_previousBoardText.empty())
        {
            // Case 1: Text is identical - continue from where we left off
            if (fullText == m_previousBoardText)
            {
                shouldContinue = true;
                // Keep the current timer position (don't change m_dialogueTimer)
            }
            // Case 2: New text extends previous text - continue typing the new portion
            else if (fullText.size() > m_previousBoardText.size() &&
                     fullText.substr(0, m_previousBoardText.size()) == m_previousBoardText)
            {
                shouldContinue = true;
                // Continue from where we left off
                m_dialogueTimer = (float)m_previousBoardTextLength / m_dialogueTextRate;
            }
        }

        if (!shouldContinue)
        {
            // Different text - reset timer
            m_dialogueTimer = 0.0f;
        }

        lastFrame = currentFrame;
    }

    // Accumulate time
    m_dialogueTimer += deltaTime;

    // Calculate how many characters to show based on elapsed time
    size_t charsToShow = static_cast<size_t>(m_dialogueTimer * m_dialogueTextRate);

    // Clamp to total text length
    if (charsToShow > fullText.size())
        charsToShow = fullText.size();

    // Extract substring from start to current position
    std::string displayedText = fullText.substr(0, charsToShow);

    if (instantRender)
    {
        m_dialogueTimer = (float)fullText.size() / m_dialogueTextRate;
        charsToShow = fullText.size();
        displayedText = fullText;
    }

    // Store current state for next board comparison
    m_previousBoardText = fullText;
    m_previousBoardTextLength = charsToShow;

    // Set the text component to display
    textComp.text = displayedText;
}


void DialogueManager::AdvanceDialogue(float deltaTime, TextRenderComponent& textComp, Transform& textTransform, int currentFrame)
{
    auto it = dialogueMap.find(currentFrame);
    if (it == dialogueMap.end())
    {
        textComp.text = "";
        return;
    }
    std::string fullText = it->second;
    textComp.text = fullText;

}


void DialogueManager::Reset()
{
    m_dialogueTimer = 0.0f;
    lastFrame = -1;
    m_lastPanel = -1;
    m_previousBoardTextLength = 0;
    m_previousBoardText = "";
}

bool DialogueManager::IsTextFinished(TextRenderComponent& textComp, int currentFrame)
{
    auto it = dialogueMap.find(currentFrame);
    if (it == dialogueMap.end())
        return true; // No text means finish by default

    std::string fullText = it->second;

    return textComp.text == fullText;
}

// Panel-based text rendering
void DialogueManager::HandlePanelTextRender(float deltaTime, TextRenderComponent& textComp, Transform& textTransform, int currentPanel, bool instantRender)
{
    auto it = panelDialogueMap.find(currentPanel);
    if (it == panelDialogueMap.end())
    {
        textComp.text = "";
        return;
    }
    std::string fullText = it->second;

    if (currentPanel != lastFrame)  // Using lastFrame to track panel changes
    {
        m_dialogueTimer = 0.0f;
        lastFrame = currentPanel;
    }

    // Accumulate time
    m_dialogueTimer += deltaTime;

    // Calculate how many characters to show based on elapsed time
    size_t charsToShow = static_cast<size_t>(m_dialogueTimer * m_dialogueTextRate);

    // Clamp to total text length
    if (charsToShow > fullText.size())
        charsToShow = fullText.size();

    // Extract substring from start to current position
    std::string displayedText = fullText.substr(0, charsToShow);

    if (instantRender)
    {
        m_dialogueTimer = (float)fullText.size() / m_dialogueTextRate;
        displayedText = fullText;
    }

    // Set the text component to display
    textComp.text = displayedText;
}

bool DialogueManager::IsTextFinishedForPanel(TextRenderComponent& textComp, int currentPanel)
{
    // Try panel-based dialogue first
    auto it = panelDialogueMap.find(currentPanel);
    if (it != panelDialogueMap.end())
    {
        std::string fullText = it->second;
        return textComp.text == fullText;
    }

    // Fall back to frame-based dialogue (currentPanel is actually currentFrame)
    auto frameIt = dialogueMap.find(currentPanel);
    if (frameIt != dialogueMap.end())
    {
        std::string fullText = frameIt->second;
        return textComp.text == fullText;
    }

    return true; // No text means finish by default
}

void DialogueManager::CompleteTextImmediately(TextRenderComponent& textComp, int currentPanel)
{
    // Try panel-based dialogue first
    auto it = panelDialogueMap.find(currentPanel);
    if (it != panelDialogueMap.end())
    {
        std::string fullText = it->second;
        textComp.text = fullText;
        m_dialogueTimer = (float)fullText.size() / m_dialogueTextRate;
        return;
    }

    // Fall back to frame-based dialogue (currentPanel is actually currentFrame)
    auto frameIt = dialogueMap.find(currentPanel);
    if (frameIt != dialogueMap.end())
    {
        std::string fullText = frameIt->second;
        textComp.text = fullText;
        m_dialogueTimer = (float)fullText.size() / m_dialogueTextRate;
        m_previousBoardText = fullText;
        m_previousBoardTextLength = fullText.size();
    }
}



