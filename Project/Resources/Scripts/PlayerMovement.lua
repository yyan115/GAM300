-- PlayerMovement.lua

require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
_G.player_is_attacking = _G.player_is_attacking or false

-- Simple atan2 implementation for Lua versions without math.atan2
local function atan2(y, x)
    if x > 0 then
        return math.atan(y / x)
    elseif x < 0 and y >= 0 then
        return math.atan(y / x) + math.pi
    elseif x < 0 and y < 0 then
        return math.atan(y / x) - math.pi
    elseif x == 0 and y > 0 then
        return math.pi / 2
    elseif x == 0 and y < 0 then
        return -math.pi / 2
    else
        return 0
    end
end

local function getWorldPosition(self)
    local ok, a, b, c = pcall(self.GetPosition, self)
    if not ok then
        print("[LUA][PlayerMovement] GetPosition failed:", a)
        return { x = 0, y = 0, z = 0 }
    end

    if type(a) == "table" then
        local t = a
        if t.x and t.y and t.z then
            return { x = t.x, y = t.y, z = t.z }
        elseif t[1] and t[2] and t[3] then
            return { x = t[1], y = t[2], z = t[3] }
        else
            return { x = 0, y = 0, z = 0 }
        end
    end

    return {
        x = a or 0,
        y = b or 0,
        z = c or 0
    }
end

local function setWorldPosition(self, x, y, z)
    local ok, err = pcall(function()
        self:SetPosition(x, y, z)
    end)
    if not ok then
        print("[LUA][PlayerMovement] SetPosition(x,y,z) failed, trying table version. Error:", err)
        pcall(function()
            self:SetPosition({ x = x, y = y, z = z })
        end)
    end
end

return Component {
    mixins = { TransformMixin },

    fields = {
        name       = "PlayerMovement",
        moveSpeed  = 2.0,
        jumpSpeed  = 5.0,
        gravity    = -9.8,
    },

    Awake = function(self)
        print("[LUA][PlayerMovement] Awake")
        self._velY             = 0.0
        self._groundY          = 0.0
        self._isGrounded       = true
        self._currentAnimState = "idle"
        self._wasAttacking     = false   -- <=== track previous frame's attack flag

        -- Cache the model's original rotation as our base
        if self.GetRotation then
            local rx, ry, rz = self:GetRotation()
            self._baseRotX, self._baseRotY, self._baseRotZ = rx or 0.0, ry or 0.0, rz or 0.0
            print(string.format(
                "[LUA][PlayerMovement] Base rotation: rx=%.2f ry=%.2f rz=%.2f",
                self._baseRotX, self._baseRotY, self._baseRotZ
            ))
        else
            self._baseRotX, self._baseRotY, self._baseRotZ = 0.0, 0.0, 0.0
        end

        local a, b, c = self:GetPosition()
        print("[LUA][PlayerMovement] GetPosition raw returns:", a, b, c)

        if self.GetComponent then
            self._anim = self:GetComponent("AnimationComponent")
            if self._anim then
                print("[LUA][PlayerMovement] Found AnimationComponent")
                -- Start from T-pose
                Animation.Stop(self._anim)
            else
                print("[LUA][PlayerMovement] No AnimationComponent on player")
            end
        end
    end,

    Start = function(self)
        print("[LUA][PlayerMovement] Start")

        local pos = getWorldPosition(self)
        print(string.format("[LUA][PlayerMovement] Start position: x=%.3f y=%.3f z=%.3f", pos.x, pos.y, pos.z))

        self._groundY = pos.y

        if event_bus and event_bus.publish then
            event_bus.publish("player_position", {
                x = pos.x,
                y = pos.y,
                z = pos.z,
            })
        end
    end,

    Update = function(self, dt)
        --------------------------------------
        -- 0) Attack lock
        --------------------------------------
        local isAttacking  = _G.player_is_attacking == true
        local wasAttacking = self._wasAttacking == true

        --------------------------------------
        -- 1) Horizontal input (WASD)
        --------------------------------------
        local moveX, moveZ = 0.0, 0.0

        if not isAttacking then
            if Input.GetKey(Input.Key.W) then
                moveZ = moveZ + 1.0   -- forward (+Z)
            end
            if Input.GetKey(Input.Key.S) then
                moveZ = moveZ - 1.0   -- backward (-Z)
            end
            if Input.GetKey(Input.Key.A) then
                moveX = moveX + 1.0   -- left
            end
            if Input.GetKey(Input.Key.D) then
                moveX = moveX - 1.0   -- right
            end
        end

        local len = math.sqrt(moveX * moveX + moveZ * moveZ)
        if len > 0.0001 then
            moveX = moveX / len
            moveZ = moveZ / len
        else
            moveX, moveZ = 0.0, 0.0
        end

        local speed = self.moveSpeed or 5.0
        local dx = moveX * speed * dt
        local dz = moveZ * speed * dt

        local isMoving = (moveX ~= 0 or moveZ ~= 0)

        --------------------------------------
        -- 2) Animation control
        --------------------------------------
        if self._anim and not isAttacking then
            if isMoving then
                -- Force re-playing walk if:
                --  - we weren't already in "walk", OR
                --  - we *just finished* an attack this frame.
                if self._currentAnimState ~= "walk" or wasAttacking then
                    Animation.PlayClip(self._anim, 0, true) -- RUN/WALK clip
                    self._currentAnimState = "walk"
                end
            else
                if self._currentAnimState ~= "idle" then
                    Animation.Pause(self._anim)
                    self._currentAnimState = "idle"
                end
            end
        end

        --------------------------------------
        -- 3) Rotate to face movement direction
        -- Currently moving left, right and back will flip the player
        -- Comment out this chunk to have player always face forward
        --------------------------------------
        if isMoving and not isAttacking and self.SetRotation then
            -- Use the stored base rotation to keep the model upright
            local baseRx = self._baseRotX or 0.0
            local baseRy = self._baseRotY or 0.0
            local baseRz = self._baseRotZ or 0.0

            -- Compute yaw from movement direction (moveX, moveZ)
            -- Here, +Z is "forward", so (y=moveX, x=moveZ) is correct
            local yawRad = atan2(moveX, moveZ)
            local yawDeg = math.deg(yawRad)

            -- Rotate around Y relative to base orientation
            local newRy = baseRy + yawDeg

            self:SetRotation(baseRx, newRy, baseRz)
        end

        --------------------------------------
        -- 4) Jump + gravity
        --------------------------------------
        local pos = getWorldPosition(self)

        if self._isGrounded and not isAttacking and Input.GetKeyDown(Input.Key.Space) then
            self._velY       = self.jumpSpeed or 5.0
            self._isGrounded = false
            print("[LUA][PlayerMovement] Jump!")
        end

        if not self._isGrounded then
            self._velY = self._velY + (self.gravity or -20.0) * dt
        end

        local dy    = self._velY * dt
        local newY  = pos.y + dy

        if newY <= self._groundY then
            newY             = self._groundY
            self._velY       = 0.0
            self._isGrounded = true
        end

        --------------------------------------
        -- 5) Apply movement
        --------------------------------------
        local newX = pos.x + dx
        local newZ = pos.z + dz

        setWorldPosition(self, newX, newY, newZ)

        --------------------------------------
        -- 6) Broadcast position for camera
        --------------------------------------
        if event_bus and event_bus.publish then
            local p = getWorldPosition(self)
            event_bus.publish("player_position", {
                x = p.x,
                y = p.y,
                z = p.z,
            })
        end

        --------------------------------------
        -- 7) Store attack flag for next frame
        --------------------------------------
        self._wasAttacking = isAttacking
    end,
}
