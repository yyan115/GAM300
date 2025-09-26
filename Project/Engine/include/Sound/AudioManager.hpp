/*********************************************************************************
* @File			AudioManager.hpp
* @Author		Ernest Ho, h.yonghengernest@digipen.edu
* @Co-Author	-
* @Date			17/9/2025
* @Brief		This is the Declaration of Audio Manager Class
*
* Copyright (C) 20xx DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/
#pragma once

#include "pch.h"

struct FMOD_SYSTEM;
struct FMOD_SOUND;
struct FMOD_CHANNEL;
struct FMOD_CHANNELGROUP;

class AudioManager
{
public:
	static AudioManager& GetInstance() {
		static AudioManager instance;
		return instance;
	}

	// Static access to the singleton instance
	static bool StaticInitalize();     // calls instance().initialize()
    static void StaticShutdown();       // calls instance().shutdown()
    static void StaticUpdate();         // calls instance().update()

    // (Optional static helpers if you want to use the same style elsewhere)
    //static bool StaticLoadSound(const std::string& name, const std::string& file, bool loop=false);
    //static bool StaticPlaySound(const std::string& name, float vol=1.f, float pitch=1.f);
    static void StaticStopAllSounds();
    static void StaticSetMasterVolume(float v);

	AudioManager();
	~AudioManager();

	// Initialize and cleanup
	bool Initialize();
	void Shutdown();
	void Update(); // Call this every frame

	// Sound loading and management
	FMOD_SOUND* LoadSound(const std::string& assetPath);
	void UnloadSound(FMOD_SOUND* sound, const std::string& assetPath);
	//void UnloadAllSounds();

	// Sound playback
	bool PlaySound(FMOD_SOUND* sound, const std::string& assetPath, float volume = 1.0f, float pitch = 1.0f);
	//bool PlaySound3D(const std::string& name, float x, float y, float z, float volume = 1.0f);
	void StopSound(const std::string& assetPath);
	void StopAllSounds();
	void PauseSound(const std::string& name, bool pause = true);
	void PauseAllSounds(bool pause = true);

	// Volume and pitch control
	void SetMasterVolume(float volume);
	void SetSoundVolume(const std::string& name, float volume);
	void SetSoundPitch(const std::string& name, float pitch);

	// 3D audio settings
	//void SetListenerPosition(float x, float y, float z);
	//void SetListenerOrientation(float forwardX, float forwardY, float forwardZ, float upX, float upY, float upZ);

	// Utility functions
	//bool IsSoundLoaded(const std::string& name) const;
	bool IsSoundPlaying(const std::string& name) const;
	//std::vector<std::string> GetLoadedSounds() const;

private:
	FMOD_SYSTEM* mSystem;
	//std::unordered_map<std::string, FMOD_SOUND*> mSounds;
	std::unordered_map<std::string, FMOD_CHANNEL*> mChannels;
	
	// Helper functions
	//std::string GetFullPath(const std::string& fileName) const;
	void CheckFMODError(int result, const std::string& operation) const;
};