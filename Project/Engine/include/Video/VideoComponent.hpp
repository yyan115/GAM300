#pragma once
#include "pch.h"
#include "Reflection/ReflectionBase.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Video/cutscene.hpp"

//std::string newCutscenePath = "../../Resources/Cutscenes/Kusane_OpeningCutscene/" + videoComp.cutSceneName;



struct ENGINE_API VideoComponent {
	REFL_SERIALIZABLE
	bool enabled = true;          // Component enabled state (can be toggled in inspector)

	// --- State Control ---
	bool isPlaying = false;     // Play/Pause state			
	bool loop = false;          // Restart video when it reaches the end
	
	// --- Playback Properties ---
	float playbackSpeed = 1.0f; 
	float currentTime = 0.0f;   // Current seek position in seconds		
	float playbackduration = 0.0f;      // Total length (usually set by the Engine/Decoder)
	
	// --- CUTSCENE Asset Management ---
	std::string videoPath = ""; // Path to the video file (.mp4, .webm, etc.)
	int frameStart = 0;   ///< Starting frame
	int frameEnd = 0;     ///< Ending frame
	int activeFrame = 0;

	// Timing configuration
	float fadeDuration = 0.5f;      ///< Fade in/out between boards (seconds)
	float boardDuration = 3.0f;     ///< How long a board stays before auto-advancing (seconds)
	float panelDuration = 6.0f;     ///< How long before fading to next panel on last board (seconds)
	float skipFadeDuration = 1.0f;  ///< Fade duration when skipping or ending (seconds)

	// Legacy timing (kept for compatibility)
	float preTime = 0.0f;  ///< Delay before first frame starts
	float duration = 0.0f; ///< Active playback time
	float postTime = 0.0f; ///< Delay after last frame reached
	std::string cutSceneName = "";

	// Panel system
	int currentPanel = 1;           ///< Current panel (1-indexed)
	float boardTimer = 0.0f;        ///< Timer for auto-advancing boards
	bool isLastBoardInPanel = false; ///< Whether current board is last in its panel


	// --- Rendering Data ---
	// This ID links to the Engine's VideoSystem texture/resource
	uint32_t textureID = 0;     
	
	// --- Synchronization Flags ---
	bool asset_dirty = false;   // Set to true when videoPath changes 
	bool seek_dirty = false;    // Set to true if currentTime is modified manually (seeking)

	bool cutsceneEnded = false;	//To change scene when toggled.

	VideoComponent() = default;
	~VideoComponent() = default;

	bool ProcessMetaData(std::string resourcePath);

	bool ProcessDialogueData(std::string dialoguePath);

	std::string PadNumber(int num);


	//DIALOGUE ASSET MANAGEMENT
	std::string dialoguePath = "";		//For reference/display purposes

	std::unordered_map<int, std::string> dialogueMap;       // Frame-based dialogue (legacy)
	std::unordered_map<int, std::string> panelDialogueMap;  // Panel-based dialogue (new)


};