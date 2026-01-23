#include "pch.h"
#include "Video/cutscenelayer.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/TagComponent.hpp"
#include "Transform/TransformComponent.hpp"


void DialogueManager::HandleTextRender(float deltaTime, TextRenderComponent& textComp, Transform& textTransform, int currentFrame, bool instantRender)
{
    auto it = dialogueMap.find(currentFrame);
    if (it == dialogueMap.end())
    {return;}
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


    //float charWidth = 0.02f;
    //// Assuming charWidth is 15.0f and charsToShow is your typewriter count
    //float currentTextWidth = (float)charsToShow * 15.0f;

    //// Calculate the far left boundary of the parent
    //float parentLeftEdge = -(boxTransform.localScale.x / 2.0f);

    //// Calculate the offset so the middle-drawn child stays inside the edge
    //float childHalfWidth = currentTextWidth / 2.0f;

    //// Set local position
    //textTransform.localPosition.x = parentLeftEdge + childHalfWidth;
    //textTransform.localPosition.y = 0.0f; // Vertically centered
    //textTransform.isDirty = true;

    //NEED TO ASK WHOEVER WHO DID TEXT RENDER COMPONENT TO IMPLEMENT "NEXT LINE" TEXT RENDER
    //Will be good if we can set our own constraint


    
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



