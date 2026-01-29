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
        m_dialogueTimer = 0.0f;
        lastFrame = currentFrame;
    }

    // Accumulate time
    m_dialogueTimer += deltaTime;

    // Calculate how many characters to show based on elapsed time
    // textSpeed controls how fast letters appear (characters per second)
    size_t charsToShow = static_cast<size_t>(m_dialogueTimer * m_dialogueTextRate);

    // Clamp to total text length
    if (charsToShow > fullText.size())
        charsToShow = fullText.size();

    // Extract substring from start to current position
    std::string displayedText = fullText.substr(0, charsToShow);

    if (instantRender)
    {
        m_dialogueTimer = (float)fullText.size() / m_dialogueTextRate;
    }

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
}

bool DialogueManager::IsTextFinished(TextRenderComponent& textComp, int currentFrame)
{
    auto it = dialogueMap.find(currentFrame);
    if (it == dialogueMap.end())
        return true; // No text means finish by default

    std::string fullText = it->second;

    return textComp.text == fullText;
}



