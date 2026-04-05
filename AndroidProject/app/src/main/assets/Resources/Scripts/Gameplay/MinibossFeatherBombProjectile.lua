-- Resources/Scripts/Gameplay/FeatherBombProjectile.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local function atan2(y, x)
    local ok, v = pcall(math.atan, y, x)
    if ok and type(v) == "number" then return v end
    if x > 0 then return math.atan(y / x) end
    if x < 0 and y >= 0 then return math.atan(y / x) + math.pi end
    if x < 0 and y < 0 then return math.atan(y / x) - math.pi end
    if x == 0 and y > 0 then return math.pi / 2 end
    if x == 0 and y < 0 then return -math.pi / 2 end
    return 0
end

local function lerp(a, b, t)
    return a + (b - a) * t
end

-- Easing function so the projectile slows down as it reaches the apex
local function easeOutQuad(t)
    return 1.0 - (1.0 - t) * (1.0 - t)
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

-- Spherical Linear Interpolation for smooth 3D rotations
local function quatSlerp(q1, q2, t)
    -- Calculate dot product
    local dot = q1.w*q2.w + q1.x*q2.x + q1.y*q2.y + q1.z*q2.z
    
    -- If dot is negative, negate one quaternion to take the shortest path
    local q3 = { w = q2.w, x = q2.x, y = q2.y, z = q2.z }
    if dot < 0 then
        dot = -dot
        q3.w = -q3.w; q3.x = -q3.x; q3.y = -q3.y; q3.z = -q3.z
    end
    
    -- If they are practically identical, use standard linear interp
    if dot > 0.9995 then
        local res = {
            w = q1.w + t*(q3.w - q1.w),
            x = q1.x + t*(q3.x - q1.x),
            y = q1.y + t*(q3.y - q1.y),
            z = q1.z + t*(q3.z - q1.z)
        }
        local len = math.sqrt(res.w*res.w + res.x*res.x + res.y*res.y + res.z*res.z)
        return { w = res.w/len, x = res.x/len, y = res.y/len, z = res.z/len }
    end
    
    -- Spherical interpolation math
    local theta_0 = math.acos(dot)
    local theta = theta_0 * t
    local sin_theta = math.sin(theta)
    local sin_theta_0 = math.sin(theta_0)
    
    local s0 = math.cos(theta) - dot * sin_theta / sin_theta_0
    local s1 = sin_theta / sin_theta_0
    
    return {
        w = (s0 * q1.w) + (s1 * q3.w),
        x = (s0 * q1.x) + (s1 * q3.x),
        y = (s0 * q1.y) + (s1 * q3.y),
        z = (s0 * q1.z) + (s1 * q3.z)
    }
end

return Component {
    mixins = { TransformMixin },

    fields = {
        RiseDuration = 0.6,    -- How long it takes to fly diagonally up
        RiseHeight = 4.5,      -- How high above the target it stops
        HangDuration = 0.5,    -- How long it freezes in the air
        TurnDuration = 0.15,   -- How fast it aims downward during the Hang state
        FallSpeed = 35.0,      -- How fast it crashes down
        
        ExplosionDamage = 2,
        ExplosionPrefabPath = "Resources/Prefabs/MinibossFeatherBombExplosion.prefab",
        FeatherBombTargetPrefabPath = "Resources/Prefabs/FeatherBombTarget.prefab",
        BossArenaFloorY = 3.6,
    },

    Awake = function(self)
        self.state = "HIDDEN"
        self.timer = 0
    end,

    Start = function(self)
        -- Check the global blackboard for launch instructions addressed to us!
        if _G.PendingFeatherBombs and _G.PendingFeatherBombs[self.entityId] then
            local data = _G.PendingFeatherBombs[self.entityId]

            --print("[MinibossFeatherBombProjectile] Read blackboard, launching now!")

            self:Launch(
                data.sx, data.sy, data.sz,
                data.tx, data.ty, data.tz,
                data.targetCell
            )

            -- Clean up the note so we don't leak memory
            _G.PendingFeatherBombs[self.entityId] = nil
        else
            --print("[MinibossFeatherBombProjectile] Warning: Woke up but found no launch data!")
        end

        -- Instantiate the Feather Bomb Target prefab
        self._featherBombTargetPrefabId = Prefab.InstantiatePrefab(self.FeatherBombTargetPrefabPath)
        -- Make the target follow the projectile's x and z position and set the y position to be the miniboss arena floor level
        local featherBombTargetTr = GetComponent(self._featherBombTargetPrefabId, "Transform")
        local curX, curY, curZ = self:GetPosition()
        featherBombTargetTr.localPosition.x = curX
        featherBombTargetTr.localPosition.z = curZ
        featherBombTargetTr.localPosition.y = self.BossArenaFloorY
        featherBombTargetTr.isDirty = true
    end,

    -- This will be called by the Boss AI to start the sequence
    Launch = function(self, sx, sy, sz, tx, ty, tz, targetCell)
        self.startX, self.startY, self.startZ = sx, sy, sz
        self.targetX, self.targetY, self.targetZ = tx, ty, tz
        
        self.apexY = ty + self.RiseHeight
        self.targetCell = targetCell
        
        self:SetPosition(sx, sy, sz)
        
        -- ========================================================
        -- PRE-CALCULATE ROTATIONS (Adjusted for +Y Forward Model)
        -- ========================================================
        local dx = tx - sx
        local dz = tz - sz
        local distXZ = math.sqrt(dx*dx + dz*dz)
        if distXZ < 0.001 then distXZ = 0.001 end -- Prevent division by zero
        
        -- 1. Yaw remains exactly the same (turning left/right toward target)
        local yaw = math.deg(atan2(dx, dz))
        
        -- 2. Rise Pitch
        -- Calculate the true angle above the horizon...
        local riseDy = self.apexY - sy
        local angleAboveHorizon = math.deg(atan2(riseDy, distXZ))
        
        -- ...then subtract it from 90 because your model starts pointing straight UP (0 deg)
        local risePitch = 90.0 - angleAboveHorizon
        
        -- 3. Fall Pitch
        -- 180 degrees perfectly flips a +Y model upside down so it points straight into the ground
        local fallPitch = 180.0

        -- Lock in the Quaternions
        self.riseQuat = eulerToQuat(risePitch, yaw, 0)
        self.fallQuat = eulerToQuat(fallPitch, 0, 0)

        -- Apply the rising rotation immediately
        self:SetRotation(self.riseQuat.w, self.riseQuat.x, self.riseQuat.y, self.riseQuat.z)

        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("miniboss_sfx", {
                sfxType = "explosionSkillStart",
            })
        end

        self.state = "RISING"
        self.timer = 0
    end,

    Update = function(self, dt)
        if self.state == "HIDDEN" or self.state == "DEAD" then return end

        local dtSec = dt
        if dtSec > 1.0 then dtSec = dtSec * 0.001 end
        self.timer = self.timer + dtSec

        local curX, curY, curZ = self:GetPosition()

        -- ==========================================
        -- STATE 1: Fly diagonally up
        -- ==========================================
        if self.state == "RISING" then
            local t = math.min(self.timer / self.RiseDuration, 1.0)
            local easeT = easeOutQuad(t)

            curX = lerp(self.startX, self.targetX, t)
            curZ = lerp(self.startZ, self.targetZ, t)
            curY = lerp(self.startY, self.apexY, easeT)
            self:SetPosition(curX, curY, curZ)

            if t >= 1.0 then
                self.state = "HANGING"
                self.timer = 0
                -- Notice we removed the instant SetRotation snap from here!
            end

        -- ==========================================
        -- STATE 2: Freeze in the air and pivot downward
        -- ==========================================
        elseif self.state == "HANGING" then
            
            -- Calculate how far along the turn animation we are (0.0 to 1.0)
            local turnT = math.min(self.timer / self.TurnDuration, 1.0)
            
            -- Smoothly interpolate between the upward angle and the downward angle
            local currentQuat = quatSlerp(self.riseQuat, self.fallQuat, turnT)
            
            -- Apply the interpolated rotation
            self:SetRotation(currentQuat.w, currentQuat.x, currentQuat.y, currentQuat.z)

            -- Wait out the rest of the hang time before dropping
            if self.timer >= self.HangDuration then
                self.state = "FALLING"
            end

        -- ==========================================
        -- STATE 3: Crash straight down
        -- ==========================================
        elseif self.state == "FALLING" then
            curY = curY - (self.FallSpeed * dtSec)
            self:SetPosition(self.targetX, curY, self.targetZ)

            if curY <= self.targetY then
                self:Detonate()
            end
        end

        -- Make the target follow the projectile's x and z position and set the y position to be the miniboss arena floor level
        local featherBombTargetTr = GetComponent(self._featherBombTargetPrefabId, "Transform")
        featherBombTargetTr.localPosition.x = curX
        featherBombTargetTr.localPosition.z = curZ
        featherBombTargetTr.localPosition.y = self.BossArenaFloorY
        featherBombTargetTr.isDirty = true
    end,

    -- -- Helper to calculate Pitch and Yaw to look at a 3D point
    -- _PointTowards = function(self, tx, ty, tz, cx, cy, cz)
    --     -- 1. Calculate PITCH based on current position (to tilt downward)
    --     local dx = tx - cx
    --     local dy = ty - cy
    --     local dz = tz - cz
        
    --     local distXZ = math.sqrt(dx*dx + dz*dz)
    --     if distXZ < 0.001 and math.abs(dy) < 0.001 then return end

    --     -- Use our custom atan2 wrapper!
    --     local pitch = math.deg(atan2(-dy, distXZ))

    --     -- 2. Calculate YAW based on the overall flight path (Start to Target)
    --     -- This prevents the glitch at the apex!
    --     local flightDx = self.targetX - self.startX
    --     local flightDz = self.targetZ - self.startZ
    --     local yaw = math.deg(atan2(flightDx, flightDz))

    --     local q = eulerToQuat(pitch, yaw, 0)
    --     self:SetRotation(q.w, q.x, q.y, q.z)
    -- end,

    Detonate = function(self)
        self.state = "DEAD"

        -- 1. Spawn visual explosion
        local expId = Prefab.InstantiatePrefab(self.ExplosionPrefabPath)
        local expTr = GetComponent(expId, "Transform")
        if expTr then
            expTr.localPosition.x = self.targetX
            expTr.localPosition.y = self.targetY
            expTr.localPosition.z = self.targetZ
            expTr.isDirty = true
        end

        -- -- 2. Trigger the damage logic via Event Bus
        -- if _G.event_bus and _G.event_bus.publish then
        --     _G.event_bus.publish("boss_rain_explosives", {
        --         entityId = self.entityId,
        --         cells = { self.targetCell },
        --         dmg = self.ExplosionDamage,
        --         step = 4.0, cx = 0.0, cz = 0.0 -- Grid data needed by your health script
        --     })
        -- end

        -- 3. Destroy this projectile and the target prefab
        if Engine and Engine.DestroyEntity then
            Engine.DestroyEntity(self.entityId)
            Engine.DestroyEntity(self._featherBombTargetPrefabId)
        end
    end,
}