require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

-- Helper function for linear interpolation
local function lerp(a, b, t)
    return a + (b - a) * t
end

local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0)   * 0.5
    local r = math.rad(roll or 0)  * 0.5

    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)

    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

local function multiplyQuat(q1, q2)
    return {
        w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
        x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
    }
end

return Component {
    mixins = { TransformMixin },

    fields = {
        CastingRotationSpeed = 360.0,
        FeatherTargetRotationX = 70.0, 
        LaunchRotationSpeed = 1080.0, 
        ProjectileSpeed = 25.0,     -- Might want to increase speed so the snap feels heavier
        CastDelay = 3.0,
        
        -- [NEW] Converge Settings
        ConvergeDuration = 0.5,     -- How long it takes for feathers to fold inward during casting
        
        -- Wind-up Settings
        WindupDuration = 0.2,       -- How long the pull-back takes
        WindupPullbackDistance = 0.6, -- How far backward it jerks before shooting

        -- SkillLight color settings
        SkillLightEntityName = "SkillLight",
        SkillLightInitialDiffuseR = 255,
        SkillLightInitialDiffuseG = 170,
        SkillLightInitialDiffuseB = 170,

        SkillLightCastedDiffuseR = 255,
        SkillLightCastedDiffuseG = 0,
        SkillLightCastedDiffuseB = 0,
    },

    Awake = function(self)
        self._castDelay = self.CastDelay
        self._windupTimer = self.WindupDuration
        
        -- States: 0 = Casting, 1 = Windup, 2 = Launched
        self._state = 0 
        self._startSkill = false
    end,

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        
        -- Cache the original aiming rotation of the parent
        local r = self._transform.localRotation
        self._baseRotation = { w = r.w, x = r.x, y = r.y, z = r.z }
        self._currentSpinAngle = 0

        self._childFeathers = {}
        
        if Engine and Engine.GetChildrenEntities then
            local childrenIds = Engine.GetChildrenEntities(self.entityId)
            
            for _, childId in ipairs(childrenIds) do
                local childTransform = GetComponent(childId, "Transform")
                if childTransform then
                    local cr = childTransform.localRotation
                    table.insert(self._childFeathers, {
                        transform = childTransform,
                        baseRot = { w = cr.w, x = cr.x, y = cr.y, z = cr.z }
                    })
                end

                -- Get reference to the child SkillLight's PointLightComponent
                local childName = Engine.GetEntityName(childId)
                if childName == self.SkillLightEntityName then
                    local skillPointLightComp = GetComponent(childId, "PointLightComponent")
                    if skillPointLightComp then
                        print("[FeatherSkillManager] Got PointLightComponent")
                        self._skillPointLightComp = skillPointLightComp
                        self._skillPointLightComp.diffuse.x = self.SkillLightInitialDiffuseR / 255.0
                        self._skillPointLightComp.diffuse.y = self.SkillLightInitialDiffuseG / 255.0
                        self._skillPointLightComp.diffuse.z = self.SkillLightInitialDiffuseB / 255.0
                    end
                end
            end
        end
    end,

    Update = function(self, dt)
        if Input and Input.IsActionPressed and Input.IsActionJustPressed("FeatherSkill") then
            self._startSkill = true
            if self._skillPointLightComp then
                self._skillPointLightComp.enabled = true
            end
        end

        if not self._startSkill then 
            if self._skillPointLightComp then
                self._skillPointLightComp.enabled = false
            end
            return 
        end

        local currentRotationSpeed = 0

        -- Calculate Forward Vector (Needed for both Windup and Launch)
        local bw, bx, by, bz = self._baseRotation.w, self._baseRotation.x, self._baseRotation.y, self._baseRotation.z
        local fwdX = 2 * (bx * bz + bw * by)
        local fwdY = 2 * (by * bz - bw * bx)
        local fwdZ = 1 - 2 * (bx * bx + by * by)

        -- ==========================================
        -- PHASE 0: CASTING DELAY
        -- ==========================================
        if self._state == 0 then
            self._castDelay = self._castDelay - dt
            currentRotationSpeed = self.CastingRotationSpeed

            -- [FIX] Calculate how much time has passed since cast started
            local elapsedCastTime = self.CastDelay - self._castDelay
            
            -- Lerp the X rotation over ConvergeDuration (0.5s) instead of the full CastDelay
            local t = elapsedCastTime / self.ConvergeDuration
            if t < 0 then t = 0 end
            if t > 1 then t = 1 end -- Stays at 1.0 (fully folded) for the remainder of the CastDelay

            local currentXRotation = lerp(0, self.FeatherTargetRotationX, t)
            local pitchQuat = eulerToQuat(currentXRotation, 0, 0)

            for _, child in ipairs(self._childFeathers) do
                local newRot = multiplyQuat(child.baseRot, pitchQuat)
                child.transform.localRotation.w = newRot.w
                child.transform.localRotation.x = newRot.x
                child.transform.localRotation.y = newRot.y
                child.transform.localRotation.z = newRot.z
                child.transform.isDirty = true
            end

            -- Transition to Windup once the full CastDelay is over
            if self._castDelay <= 0.0 then
                self._state = 1
                
                -- Save exactly where we are so we can pull back from this point
                local pos = self._transform.localPosition
                self._windupStartPos = { x = pos.x, y = pos.y, z = pos.z }
            end

        -- ==========================================
        -- PHASE 1: WIND-UP (The Jerk Backward)
        -- ==========================================
        elseif self._state == 1 then
            self._windupTimer = self._windupTimer - dt
            currentRotationSpeed = self.LaunchRotationSpeed -- Rev up the drill spin!

            -- t goes from 0 to 1 over the windup duration
            local t = 1.0 - (self._windupTimer / self.WindupDuration)
            if t > 1 then t = 1 end
            if t < 0 then t = 0 end
            
            -- Use Quadratic Ease-In (t * t). 
            -- This makes it pull back slowly at first, then fast, making the sudden forward snap look violent.
            local easeT = t * t 

            local pos = self._transform.localPosition
            -- Subtracting the forward vector pulls it backward
            pos.x = self._windupStartPos.x - (fwdX * self.WindupPullbackDistance * easeT)
            pos.y = self._windupStartPos.y - (fwdY * self.WindupPullbackDistance * easeT)
            pos.z = self._windupStartPos.z - (fwdZ * self.WindupPullbackDistance * easeT)
            self._transform.localPosition = pos

            -- Transition to Launched
            if self._windupTimer <= 0.0 then
                print("[FeatherSkillManager] Launched!")
                self._state = 2
            end

        -- ==========================================
        -- PHASE 2: LAUNCHED (Fly Forward)
        -- ==========================================
        elseif self._state == 2 then
            if self._skillPointLightComp then
                self._skillPointLightComp.diffuse.x = self.SkillLightCastedDiffuseR / 255.0
                self._skillPointLightComp.diffuse.y = self.SkillLightCastedDiffuseG / 255.0
                self._skillPointLightComp.diffuse.z = self.SkillLightCastedDiffuseB / 255.0
            end

            currentRotationSpeed = self.LaunchRotationSpeed

            local moveStep = self.ProjectileSpeed * dt
            local pos = self._transform.localPosition

            pos.x = pos.x + (fwdX * moveStep)
            pos.y = pos.y + (fwdY * moveStep)
            pos.z = pos.z + (fwdZ * moveStep)

            self._transform.localPosition = pos
        end

        -- ==========================================
        -- PARENT ROTATION LOGIC (Runs in all phases)
        -- ==========================================
        self._currentSpinAngle = (self._currentSpinAngle + currentRotationSpeed * dt) % 360
        local spinQuat = eulerToQuat(0, 0, self._currentSpinAngle)
        local parentNewRot = multiplyQuat(self._baseRotation, spinQuat)
        
        self._transform.localRotation.w = parentNewRot.w
        self._transform.localRotation.x = parentNewRot.x
        self._transform.localRotation.y = parentNewRot.y
        self._transform.localRotation.z = parentNewRot.z
        self._transform.isDirty = true
    end,

    OnDisable = function(self)

    end,
}