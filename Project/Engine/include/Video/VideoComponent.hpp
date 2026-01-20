#pragma once
#include "pch.h"
#include "Reflection/ReflectionBase.hpp"


struct VideoComponent {
	REFL_SERIALIZABLE
	bool enabled = true;          // Component enabled state (can be toggled in inspector)

	// --- State Control ---
	bool isPlaying = false;     // Play/Pause state
	bool loop = false;          // Restart video when it reaches the end
	
	// --- Playback Properties ---
	float playbackSpeed = 1.0f; 
	float currentTime = 0.0f;   // Current seek position in seconds		
	float duration = 0.0f;      // Total length (usually set by the Engine/Decoder)
	
	// --- Asset Management ---
	std::string videoPath = ""; // Path to the video file (.mp4, .webm, etc.)
	    
	// --- Rendering Data ---
	// This ID links to the Engine's VideoSystem texture/resource
	uint32_t textureID = 0;     
	
	// --- Synchronization Flags ---
	bool asset_dirty = false;   // Set to true when videoPath changes 
	bool seek_dirty = false;    // Set to true if currentTime is modified manually (seeking)

	VideoComponent() = default;
	~VideoComponent() = default;
};