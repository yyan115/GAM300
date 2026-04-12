#pragma once
#include "pch.h"
#include "Reflection/ReflectionBase.hpp"
#include "ECS/Entity.hpp"
#include <cstdint>
#include <string>
#include <vector>

// A single board (frame) in a cutscene sequence
struct CutsceneBoard {
	REFL_SERIALIZABLE

	// --- Image ---
	std::string imagePath;              // Drag-drop texture path

	// --- Audio ---
	std::string sfxGuidStr;             // GUID of SFX to play when this board becomes active (empty = none)

	// --- Text ---
	std::string text;                   // Dialogue text to display
	float textSpeed = 50.0f;            // Typewriter chars/sec (0 = instant)
	bool continueText = false;          // Continue typewriter from previous board

	// --- Timing ---
	float duration = 3.0f;             // How long this board displays (seconds)
	float fadeDuration = 0.5f;         // Fade time for transition INTO this board (seconds)
	bool disableFadeOut = false;       // Skip fade-to-black when leaving this board

	// --- Blur ---
	float blurDelay = 0.0f;            // Seconds to stay sharp before blur starts
	float blurIntensity = 0.0f;        // Blur at start of board (0=sharp, 1=full)
	float blurIntensityEnd = -1.0f;    // Blur at end of board (-1=same as start, i.e. static)
	float blurRadius = 2.0f;           // Blur kernel radius
	int blurPasses = 2;                // Blur iterations
};

// Inspector-driven cutscene component (replaces old text-file-based system)
struct ENGINE_API VideoComponent {
	REFL_SERIALIZABLE

	// === Serialized Fields ===
	bool enabled = true;                        // Component enabled toggle
	std::string cutsceneName;                   // Display name

	// Entity references (GUID drag-drop from hierarchy)
	std::string textEntityGuidStr;              // Entity with TextRenderComponent
	std::string blackScreenEntityGuidStr;       // Entity with SpriteRenderComponent (fade overlay)
	std::string skipButtonEntityGuidStr;        // Entity with SpriteRenderComponent (optional)

	// Global settings
	float startDelay = 0.0f;                    // Seconds to wait before starting (e.g. wait for scene fade)
	float skipFadeDuration = 1.0f;              // Fade duration when skip pressed
	bool autoStart = false;                     // Start on scene load
	bool loop = false;                          // Restart when done
	std::string nextScenePath;                  // Scene to load on finish (empty = none)

	// Board list
	std::vector<CutsceneBoard> boards;

	// === Runtime State (NOT serialized) ===
	enum class Phase {
		Inactive,
		FadingIn,
		Displaying,
		TransitionOut,
		TransitionIn,
		EndingFade,
		Finished
	};

	Phase phase = Phase::Inactive;
	int currentBoardIndex = -1;
	float startDelayTimer = 0.0f;
	float stateTimer = 0.0f;
	float typewriterTimer = 0.0f;
	int revealedChars = 0;
	int previousBoardChars = 0;
	bool skipRequested = false;
	bool cutsceneEnded = false;
	// Set by Lua (e.g. SkipHighlight) while the skip fade is running. While true
	// the Displaying phase ignores pointer taps so the player can't keep clicking
	// through dialogue boards after they've already chosen to skip.
	bool inputLocked = false;
	uint64_t activeBoardSfxChannel = 0; // Runtime channel handle for board SFX (0 = none)

	// Resolved entity handles
	Entity textEntity = 0;
	Entity blackScreenEntity = 0;
	Entity skipButtonEntity = 0;
	bool needsInit = true;

	// Saved camera blur state (restored when cutscene ends)
	bool savedCameraBlur = false;
	bool origBlurEnabled = false;
	float origBlurIntensity = 0.0f;
	float origBlurRadius = 2.0f;
	int origBlurPasses = 2;

	// Blur continuity state (smooth transitions between boards)
	float boardElapsedTime = 0.0f;      // Blur-dedicated timer (independent of phase stateTimer)
	float lastComputedBlur = 0.0f;      // Current blur value (carries across transitions)
	float transitionBlurFrom = 0.0f;    // Blur value when transition started
	float transitionBlurTo = 0.0f;      // Target blur for incoming board

	VideoComponent() = default;
	~VideoComponent() = default;
};
