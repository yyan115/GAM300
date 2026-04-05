-- Resources/Scripts/UI/BossHealthBar.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

local function setEntityActiveByName(name, active)
    print(string.format("[setEntityActiveByName] name: %s", name))
    local ent = Engine.GetEntityByName(name)
    if not ent then return end

    local ac = GetComponent(ent, "ActiveComponent")
    if ac then
        ac.isActive = active and true or false
    end
end

return Component {
    fields = {
        followDelay = 0.5,
        followSpeed = 2.0,

        healthBarFillName = "MinibossHealthBarFillGhost",

        -- root objects to toggle on/off
        rootEntityName = "MinibossHealthBarBack",
        ghostEntityName = "MinibossHealthBarFillGhost",
        immediateEntityName = "MinibossHealthBarFill",
    },

    Awake = function(self)
        self._maxHealth = 1
        self._currentHealth = 1
        self._visible = false

        if event_bus and event_bus.subscribe then
            self._bossMaxHealthSub = event_bus.subscribe("bossMaxhealth", function(maxHealth)
                if maxHealth then
                    self._maxHealth = maxHealth
                end
            end)

            self._bossCurrentHealthSub = event_bus.subscribe("bossCurrentHealth", function(currentHealth)
                if currentHealth ~= nil then
                    self._currentHealth = currentHealth
                end
            end)

            self._bossBarVisibleSub = event_bus.subscribe("bossHealthBarVisible", function(visible)
                self._visible = visible == true
                self:SetBarVisible(self._visible)
            end)
        else
            print("[BossHealthBar] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._sprite = self:GetComponent("SpriteRenderComponent")

        local ghostEnt = Engine.GetEntityByName(self.healthBarFillName)
        if ghostEnt then
            self._ghostSprite = GetComponent(ghostEnt, "SpriteRenderComponent")
        else
            print("[BossHealthBar] WARNING: Could not find ghost bar: " .. self.healthBarFillName)
        end

        self._delayTimer = 0.0
        self._ghostFill = 1.0
        self._previousHealthPct = 1.0

        self:SetBarVisible(false)
    end,

    SetBarVisible = function(self, visible)
        setEntityActiveByName(self.rootEntityName, visible)
        setEntityActiveByName(self.ghostEntityName, visible)
        setEntityActiveByName(self.immediateEntityName, visible)

        if visible then
            local maxHealth = self._maxHealth or 1
            local currentHealth = self._currentHealth or maxHealth
            local pct = math.max(0.0, math.min(1.0, currentHealth / math.max(1, maxHealth)))

            self._ghostFill = pct
            self._previousHealthPct = pct
            self._delayTimer = 0.0

            if self._sprite then
                self._sprite.fillValue = pct
            end
            if self._ghostSprite then
                self._ghostSprite.fillValue = pct
            end
        end
    end,

    Update = function(self, dt)
        if not self._visible then return end

        local maxHealth = math.max(1, self._maxHealth or 1)
        local currentHealth = self._currentHealth or maxHealth
        local pct = math.max(0.0, math.min(1.0, currentHealth / maxHealth))

        if self._sprite then
            self._sprite.fillValue = pct
        end

        if self._ghostSprite then
            if pct < self._previousHealthPct then
                self._delayTimer = 0.0
            end
            self._previousHealthPct = pct

            if self._delayTimer >= self.followDelay then
                self._ghostFill = self._ghostFill + (pct - self._ghostFill) * (self.followSpeed * dt)
                if math.abs(self._ghostFill - pct) < 0.001 then
                    self._ghostFill = pct
                end
                self._ghostSprite.fillValue = self._ghostFill
            else
                self._delayTimer = self._delayTimer + dt
            end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._bossMaxHealthSub then event_bus.unsubscribe(self._bossMaxHealthSub) end
            if self._bossCurrentHealthSub then event_bus.unsubscribe(self._bossCurrentHealthSub) end
            if self._bossBarVisibleSub then event_bus.unsubscribe(self._bossBarVisibleSub) end
        end
    end,
}