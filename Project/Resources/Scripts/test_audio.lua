-- test_audio.lua
-- Test script demonstrating serialized audio fields and playback

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Single audio clip (set this to the GUID of an audio file, e.g. "00374a211e39d67f-000331fc3a00000b" for Danger Low Hit_SFX.wav)
        testAudioClip = "",
        
        -- Array of audio clips (like enemyHurtSFXClips)
        enemyHurtSFXClips = {},
        
        -- Other fields
        playOnStart = true,
        volume = 0.5,
        soundTimer = 0
    },

    -- Called when the script starts (after Awake)
    Start = function(self)
        -- Get the AudioComponent attached to this entity
        self.audio = self:GetComponent("AudioComponent")
        
        if self.audio then
            self.audio.enabled = true
            self.audio.PlayOnAwake = self.playOnStart
            self.audio:SetVolume(self.volume)

            -- Set the audio clip if provided
            if self.testAudioClip ~= "" then
                self.audio:SetClip(self.testAudioClip)
                print("Set audio clip to: " .. self.testAudioClip)
            else
                print("No testAudioClip set, using component's existing clip")
            end

            print("AudioComponent enabled: " .. tostring(self.audio.enabled))
            print("AudioComponent PlayOnAwake set to: " .. tostring(self.audio.PlayOnAwake))
            print("AudioComponent Volume set to: " .. tostring(self.audio.Volume))
        else
            print("Warning: No AudioComponent found on entity")
        end

        -- Initialize timer for sound playback
        self.soundTimer = 0
    end,

    -- Called every frame
    Update = function(self, dt)
        -- Update timer
        self.soundTimer = self.soundTimer + dt
        
        -- Play a random hurt sound every 2 seconds for testing
        if self.audio and #self.enemyHurtSFXClips > 0 and self.soundTimer >= 2.0 then
            self:PlayRandomHurtSound()
            self.soundTimer = 0  -- Reset timer
        end
    end,

    -- Custom function to play a random hurt sound
    PlayRandomHurtSound = function(self)
        if self.audio and #self.enemyHurtSFXClips > 0 then
            local index = math.random(1, #self.enemyHurtSFXClips)
            self.audio:PlayOneShot(self.enemyHurtSFXClips[index])
            print("Playing random hurt sound: " .. tostring(index) .. " (every 2 seconds)")
        else
            print("No audio component or no hurt clips available")
        end
    end,

    -- Function to stop audio
    StopAudio = function(self)
        if self.audio then
            self.audio:stop()
            print("Audio stopped")
        end
    end,

    -- Function to test all clips in sequence
    TestAllClips = function(self)
        if self.audio and #self.enemyHurtSFXClips > 0 then
            print("Testing all " .. #self.enemyHurtSFXClips .. " hurt clips")
            for i, clipGuid in ipairs(self.enemyHurtSFXClips) do
                if clipGuid ~= "" then
                    self.audio:SetClip(clipGuid)
                    self.audio:Play()
                    -- In a real game, you'd add delays between plays
                    print("Playing clip " .. i)
                end
            end
        end
    end
}