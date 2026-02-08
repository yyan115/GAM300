require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local event_bus = _G.event_bus

return Component {
    fields = {
        fadeScreenName = "DeathScreenBG",
        respawnButtonName = "RespawnButton",
        respawnButtonHoveredName = "RespawnButtonHovered",
        isFading = false,
        fadeAlpha = 0,
        fadeDuration = 1.0,
        deathAnimationDelay = 3.0,
        playerDead = false,
    },

    Awake = function(self)
        if event_bus and event_bus.subscribe then
            print("[GameOverMenu] Subscribing to playerDead")
            self._playerDeadSub = event_bus.subscribe("playerDead", function(dead)
                if dead then
                    self._playerDead = dead
                    print("[GameOverMenu] playerDead received: ", self._playerDead)
                end
            end)
            print("[GameOverMenu] Subscription token: " .. tostring(self._playerDeadSub))

            print("[GameOverMenu] Subscribing to respawnPlayer")
            self._respawnPlayerSub = event_bus.subscribe("respawnPlayer", function(respawn)
                if respawn then
                    if self._fadeActive then
                        self._fadeActive.isActive = false
                    end
                    if self._fadeSprite then
                        -- Initialize alpha to 0
                        self._fadeSprite.alpha = 0
                    end

                    if self._respawnButtonActive then
                        self._respawnButtonActive.isActive = false
                    end
                    if self._respawnButtonSprite then
                        -- Initialize alpha to 0
                        self._respawnButtonSprite.alpha = 0
                    end

                    if self._respawnButtonHoveredActive then
                        self._respawnButtonHoveredActive.isActive = false
                    end

                    self._fadeTimer = 0
                    self._deathAnimationDelay = self.deathAnimationDelay
                    self._playerDead = false
                    -- Stop/reset BGM so it doesn't get spammed or continue playing
                    if self._BGMaudio then
                        if type(self._BGMaudio.Stop) == "function" then
                            pcall(function() self._BGMaudio:Stop() end)
                        elseif self._BGMaudio.stop ~= nil then
                            self._BGMaudio.stop = true
                        end
                        if self._BGMaudio.volume ~= nil then
                            self._BGMaudio.volume = 0
                        end
                        self._bgmStarted = false
                    end
                end
            end)
        else
            print("[GameOverMenu] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._isFading = false
        self._fadeAlpha = 0
        self._fadeTimer = 0
        self._deathAnimationDelay = self.deathAnimationDelay

        local fadeEntity = Engine.GetEntityByName(self.fadeScreenName)
        if fadeEntity then
            self._fadeActive = GetComponent(fadeEntity, "ActiveComponent")
            self._fadeSprite = GetComponent(fadeEntity, "SpriteRenderComponent")
            if self._fadeActive then
                self._fadeActive.isActive = false
            end
            if self._fadeSprite then
                -- Initialize alpha to 0
                self._fadeSprite.alpha = 0
            end
        end

        local respawnButton = Engine.GetEntityByName(self.respawnButtonName)
        if respawnButton then
            self._respawnButtonActive = GetComponent(respawnButton, "ActiveComponent")
            self._respawnButtonSprite = GetComponent(respawnButton, "SpriteRenderComponent")
            if self._respawnButtonActive then
                self._respawnButtonActive.isActive = false
            end
            if self._respawnButtonSprite then
                -- Initialize alpha to 0
                self._respawnButtonSprite.alpha = 0
            end
        end

        local respawnButtonHovered = Engine.GetEntityByName(self.respawnButtonHoveredName)
        if respawnButtonHovered then
            self._respawnButtonHoveredActive = GetComponent(respawnButtonHovered, "ActiveComponent")
            if self._respawnButtonHoveredActive then
                self._respawnButtonHoveredActive.isActive = false
            end
        end

        self._BGMaudio = GetComponent(Engine.GetEntityByName("DeathScreenBGM"), "AudioComponent")
        -- Track whether the BGM has been started to prevent repeated Play calls
        self._bgmStarted = false
    end,

    -- Update handles fade transition and scene loading
    Update = function(self, dt)
        -- If player just died, wait ~3 secs for the death animation to play before fading in the Death Screen
        if self._playerDead and self._deathAnimationDelay then
            self._deathAnimationDelay = self._deathAnimationDelay - dt
            --print("[GameOverMenu] self._deathAnimationDelay", self._deathAnimationDelay)
            if self._deathAnimationDelay <= 0 then
                self._isFading = true
                self._playerDead = false
                self._fadeActive.isActive = true
                self._respawnButtonActive.isActive = true
                self._respawnButtonHoveredActive.isActive = true
                if Screen and Screen.IsCursorLocked() then
                    Screen.SetCursorLocked(false)
                end
                    -- Stop main game BGM and ambience so DeathScreenBGM can play alone
                    local bgm1 = GetComponent(Engine.GetEntityByName("BGM1"), "AudioComponent")
                    if bgm1 then
                        if type(bgm1.Stop) == "function" then pcall(function() bgm1:Stop() end) end
                        if bgm1.volume ~= nil then bgm1.volume = 0 end
                    end
                    local ambience = GetComponent(Engine.GetEntityByName("Ambience"), "AudioComponent")
                    if ambience then
                        if type(ambience.Stop) == "function" then pcall(function() ambience:Stop() end) end
                        if ambience.volume ~= nil then ambience.volume = 0 end
                    end
                    -- Start BGM once when the death screen is shown
                    if self._BGMaudio and not self._bgmStarted then
                        if type(self._BGMaudio.Play) == "function" then
                            -- call Play method if available
                            pcall(function() self._BGMaudio:Play() end)
                        elseif self._BGMaudio.play ~= nil then
                            -- fallback: set play flag if the component uses a property
                            self._BGMaudio.play = true
                        end
                        -- prefer a looping background track if supported
                        if self._BGMaudio.loop ~= nil then
                            self._BGMaudio.loop = true
                        end
                        self._bgmStarted = true
                    end
            end
        end

        -- Handle fade in of Death Screen
        if self._isFading and self._fadeSprite then
            self._fadeTimer = self._fadeTimer + dt
            local duration = self.fadeDuration or 1.0
            self._fadeAlpha = math.min(self._fadeTimer / duration, 1.0)
            self._fadeSprite.alpha = self._fadeAlpha
            self._respawnButtonSprite.alpha = self._fadeAlpha
            if self._BGMaudio then
                self._BGMaudio.volume = self._fadeAlpha
            end
            -- Stop once fade is complete
            if self._fadeAlpha >= 1.0 then
                self._isFading = false
                -- Pause the game when death screen shows
                Time.SetPaused(true)
            end
        end
    end
}
