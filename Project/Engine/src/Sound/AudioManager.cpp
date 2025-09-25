/*********************************************************************************
* @File			AudioManager.cpp
* @Author		Ernest Ho, h.yonghengernest@digipen.edu
* @Co-Author	-
* @Date			17/9/2025
* @Brief		This is the Definition of Audio Manager Class
*
* Copyright (C) 20xx DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/

#include "pch.h"
#include "Sound/AudioManager.hpp"
#ifdef ANDROID
#include "fmod.h"
#include "fmod_errors.h"
#else
#include <fmod.h>
#include <fmod_errors.h>
#endif

AudioManager::AudioManager(): mSystem(nullptr)
{
}

AudioManager::~AudioManager()
{
	Shutdown();
}

bool AudioManager::Initialize() 
{
	// Create FMOD system with correct version
	FMOD_RESULT result = FMOD_System_Create(&mSystem, FMOD_VERSION);
	if (result != FMOD_OK) 
	{
		std::cerr << "Failed to create FMOD system: " << FMOD_ErrorString(result) <<
			std::endl;
		return false;
	}
	// Initialize FMOD system
	result = FMOD_System_Init(mSystem, 32, FMOD_INIT_NORMAL, nullptr);
	if (result != FMOD_OK) 
	{
		std::cerr << "Failed to initialize FMOD system: " << FMOD_ErrorString(result) <<
			std::endl;
		return false;
	}
	std::cout << "AudioManager initialized successfully" << std::endl;
	return true;
}

void AudioManager::Shutdown() 
{
	if (mSystem) 
	{
		// Stop all sounds
		StopAllSounds();
		// Unload all sounds
		UnloadAllSounds();
		// Close and release FMOD system
		FMOD_System_Close(mSystem);
		FMOD_System_Release(mSystem);
		mSystem = nullptr;
		std::cout << "AudioManager shutdown complete" << std::endl;
	}
}

void AudioManager::Update() 
{
	if (mSystem) 
	{
		FMOD_System_Update(mSystem);
	}
}

bool AudioManager::LoadSound(const std::string& name, const std::string& filePath, bool	loop) 
{
	if (!mSystem) 
	{
		std::cerr << "AudioManager not initialized" << std::endl;
		return false;
	}
	
	// Check if sound is already loaded
	if (mSounds.find(name) != mSounds.end()) 
	{
		std::cout << "Sound '" << name << "' is already loaded" << std::endl;
		return true;
	}
	
	std::string fullPath = GetFullPath(filePath);
	
	// Check if file exists
	if (!std::filesystem::exists(fullPath)) 
	{
		std::cerr << "Audio file not found: " << fullPath << std::endl;
		return false;
	}
	
	FMOD_SOUND* sound = nullptr;
	FMOD_MODE mode = FMOD_DEFAULT;
	
	if (loop) 
	{
		mode |= FMOD_LOOP_NORMAL;
	}
	
	FMOD_RESULT result = FMOD_System_CreateSound(mSystem, fullPath.c_str(), mode, nullptr,	&sound);
	
	if (result != FMOD_OK) 
	{
		std::cerr << "Failed to load sound '" << name << "': " << FMOD_ErrorString(result)
			<< std::endl;
		return false;
	}
	
	mSounds[name] = sound;
	std::cout << "Loaded sound: " << name << " from " << fullPath << std::endl;
	
	return true;
}

void AudioManager::UnloadSound(const std::string& name)
{
	auto it = mSounds.find(name);
	if (it != mSounds.end()) 
	{
		FMOD_Sound_Release(it->second);
		mSounds.erase(it);
		mChannels.erase(name); // Also remove any associated channel
		std::cout << "Unloaded sound: " << name << std::endl;
	}
	else 
	{
		std::cerr << "Sound '" << name << "' not found." << std::endl;
	}
}

void AudioManager::UnloadAllSounds() 
{
	for (auto& pair : mSounds) 
	{
		FMOD_Sound_Release(pair.second);
	}
	mSounds.clear();
	std::cout << "Unloaded all sounds" << std::endl;
}

bool AudioManager::PlaySound(const std::string& name, float volume, float pitch) 
{
	if (!mSystem) 
	{
		std::cerr << "AudioManager not initialized" << std::endl;
		return false;
	}
	
	auto it = mSounds.find(name);
	if (it == mSounds.end()) 
	{
		std::cerr << "Sound '" << name << "' not loaded" << std::endl;
		return false;
	}
	FMOD_CHANNEL* channel = nullptr;
	FMOD_RESULT result = FMOD_System_PlaySound(mSystem, it->second, nullptr, false, &channel);
	
	if (result != FMOD_OK) 
	{
		std::cerr << "Failed to play sound '" << name << "': " << FMOD_ErrorString(result)
			<< std::endl;
		return false;
	}
	
	// Set volume and pitch
	FMOD_Channel_SetVolume(channel, volume);
	FMOD_Channel_SetPitch(channel, pitch);

	// Store channel for later control
	mChannels[name] = channel;
	std::cout << "Playing sound: " << name << std::endl;
	return true;
}

void AudioManager::StopSound(const std::string& name) 
{
	auto it = mChannels.find(name);
	if (it != mChannels.end()) 
	{
		FMOD_Channel_Stop(it->second);
		mChannels.erase(it);
		std::cout << "Stopped sound: " << name << std::endl;
	}
	else 
	{
		std::cerr << "Sound '" << name << "' is not playing." << std::endl;
	}
}

void AudioManager::StopAllSounds() 
{
	for (auto& pair : mChannels) 
	{
		FMOD_Channel_Stop(pair.second);
	}
	mChannels.clear();
	std::cout << "Stopped all sounds" << std::endl;
}

void AudioManager::PauseSound(const std::string& name, bool pause) 
{
	auto it = mChannels.find(name);
	if (it != mChannels.end()) 
	{
		FMOD_Channel_SetPaused(it->second, pause);
		std::cout << (pause ? "Paused" : "Resumed") << " sound: " << name << std::endl;
	}
	else 
	{
		std::cerr << "Sound '" << name << "' is not playing." << std::endl;
	}
}

void AudioManager::PauseAllSounds(bool pause) 
{
	for (auto& pair : mChannels) 
	{
		FMOD_Channel_SetPaused(pair.second, pause);
	}
	std::cout << (pause ? "Paused" : "Resumed") << " all sounds" << std::endl;
}


void AudioManager::SetMasterVolume(float volume) {
	if (mSystem) 
	{
		FMOD_CHANNELGROUP* masterGroup = nullptr;
		FMOD_RESULT result = FMOD_System_GetMasterChannelGroup(mSystem, &masterGroup);
		
		if (result == FMOD_OK && masterGroup) 
		{
			FMOD_ChannelGroup_SetVolume(masterGroup, volume);
			std::cout << "Set master volume to: " << volume << std::endl;
		}
		else 
		{
			std::cerr << "Failed to get master channel group: " << FMOD_ErrorString(result) << std::endl;
		}
	}
}

void AudioManager::SetSoundVolume(const std::string& name, float volume) 
{
	auto it = mChannels.find(name);
	if (it != mChannels.end()) 
	{
		FMOD_Channel_SetVolume(it->second, volume);
		std::cout << "Set volume of sound '" << name << "' to: " << volume << std::endl;
	}
	else 
	{
		std::cerr << "Sound '" << name << "' is not playing." << std::endl;
	}
}

void AudioManager::SetSoundPitch(const std::string& name, float pitch) 
{
	auto it = mChannels.find(name);
	if (it != mChannels.end()) 
	{
		FMOD_Channel_SetPitch(it->second, pitch);
		std::cout << "Set pitch of sound '" << name << "' to: " << pitch << std::endl;
	}
	else 
	{
		std::cerr << "Sound '" << name << "' is not playing." << std::endl;
	}
}


bool AudioManager::IsSoundLoaded(const std::string& name) const 
{
	return mSounds.find(name) != mSounds.end();
}

bool AudioManager::IsSoundPlaying(const std::string& name) const 
{
	auto it = mChannels.find(name);
	if (it != mChannels.end()) 
	{
		FMOD_BOOL isPlaying = false;
		FMOD_Channel_IsPlaying(it->second, &isPlaying);
		return isPlaying != 0;
	}
	return false;
}

std::vector<std::string> AudioManager::GetLoadedSounds() const 
{
	std::vector<std::string> soundNames;
	for (const auto& pair : mSounds) 
	{
		soundNames.push_back(pair.first);
	}
	return soundNames;
}


// Additional methods implementation...
std::string AudioManager::GetFullPath(const std::string& fileName) const 
{
	// Try to find the audio file in the game-assets directory
	std::filesystem::path currentPath = std::filesystem::current_path();

	// Try different possible paths
	std::vector<std::filesystem::path> possiblePaths = {
	currentPath / "Resources" / "Audio" / "sfx" / fileName,
	currentPath / ".." / "Resources" / "Audio" / "sfx" / fileName,
	currentPath / ".." / ".." / "Resources" / "Audio" / "sfx" / fileName,
	currentPath / ".." / ".." / ".." / "Resources" / "Audio" / "sfx" / fileName
	};
	
	for (const auto& path : possiblePaths) 
	{
		if (std::filesystem::exists(path)) 
		{
			return path.string();
		}
	}
	
	// If not found, return the original filename
	return fileName;
}


void AudioManager::CheckFMODError(int result, const std::string& operation) const 
{
	FMOD_RESULT fmodResult = static_cast<FMOD_RESULT>(result);
	if (fmodResult != FMOD_OK)
	{
		std::cerr << "FMOD error during " << operation << ": " << FMOD_ErrorString(fmodResult) << std::endl;
	}
}


// Static interface implementations
bool AudioManager::StaticInitalize() 
{
	return Instance().Initialize();
}

void AudioManager::StaticShutdown() 
{
	Instance().Shutdown();
}

void AudioManager::StaticUpdate() 
{
	Instance().Update();
}

bool AudioManager::StaticLoadSound(const std::string& name, const std::string& file, bool loop) 
{
	return Instance().LoadSound(name, file, loop);
}

bool AudioManager::StaticPlaySound(const std::string& name, float vol, float pitch) 
{
	return Instance().PlaySound(name, vol, pitch);
}

void AudioManager::StaticStopAllSounds() 
{
	Instance().StopAllSounds();
}

void AudioManager::StaticSetMasterVolume(float v) 
{
	Instance().SetMasterVolume(v);
}

