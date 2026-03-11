#pragma once
#include <string>
#include <vector>
#include "Reflection/ReflectionBase.hpp"
#include "ECS/Entity.hpp"
#include "Math/Vector3D.hpp"

// How an individual entry advances to the next one
enum class DialogueScrollType : int {
    Time = 0,      // Auto-advance after timer expires
    Action = 1,    // Wait for external script to call ScrollNext()
    Trigger = 2    // Advance when player enters a trigger collider
};

// How text appears/disappears (top-level setting for entire dialogue)
enum class DialogueAppearanceMode : int {
    FadeInOut = 0,    // Fade text alpha in/out
    Typewriter = 1,   // Letter-by-letter text reveal
    Instant = 2       // Instantly appear/disappear
};

// A single dialogue line with its advancement settings
struct DialogueEntry {
    REFL_SERIALIZABLE

    std::string text;                     // The dialogue text content
    int scrollTypeID = 0;                 // Serialized int; maps to DialogueScrollType
    float autoTime = 3.0f;               // Seconds before auto-advance (Time mode only)
    std::string triggerEntityGuidStr;     // GUID string of trigger entity (Trigger mode only)
    std::string spriteEntityGuidStr;     // GUID string of sprite entity to show/hide with this entry
};

// The main dialogue component attached to an entity
struct DialogueComponent {
    REFL_SERIALIZABLE

    // === Serialized Fields (visible in inspector) ===
    std::string dialogueName;             // Unique name/ID for this dialogue
    std::string textEntityGuidStr;        // GUID of entity with TextRenderComponent
    int appearanceModeID = 0;             // Maps to DialogueAppearanceMode
    float fadeDuration = 0.5f;            // Duration of fade in/out (seconds)
    bool _deprecated_typewriter = false;  // Kept for serialization backward compat (do not use)
    float textSpeed = 50.0f;             // Characters per second (typewriter mode)
    std::vector<DialogueEntry> entries;   // The dialogue entries array
    bool autoStart = false;               // Automatically start this dialogue on scene load

    // === Runtime State (not serialized) ===
    enum class Phase {
        Inactive,
        FadingIn,
        Typing,
        Displaying,
        FadingOut,
        Finished
    };

    Phase phase = Phase::Inactive;
    int currentIndex = -1;                // -1 = not active, 0..N = current entry
    float stateTimer = 0.0f;             // General-purpose timer (fade progress, auto-advance countdown)
    float typewriterTimer = 0.0f;        // Timer for typewriter character reveal
    int revealedChars = 0;               // Number of characters currently revealed

    Entity textEntity = 0;               // Resolved at runtime from textEntityGuidStr
    bool triggerActivated = false;        // Set by trigger callback
    bool scrollNextRequested = false;     // Set by DialogueManager.ScrollNext()

    Vector3D originalTextColor{ 1.0f, 1.0f, 1.0f }; // Saved original color for fade restoration
    bool registeredWithManager = false;   // Whether this dialogue is registered with DialogueManager
    bool autoStartFired = false;          // Whether auto-start has already fired
};
