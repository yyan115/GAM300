-- PlayerMovement.lua

require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

-- helper: normalize whatever GetPosition returns into {x,y,z}
local function getWorldPosition(self)
    local ok, a, b, c = pcall(self.GetPosition, self)
    if not ok then
        print("[LUA][PlayerMovement] GetPosition failed:", a)
        return { x = 0, y = 0, z = 0 }
    end

    -- case 1: returns a table (Vector-like)
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

    -- case 2: returns three numbers (x, y, z)
    return {
        x = a or 0,
        y = b or 0,
        z = c or 0
    }
end

-- helper: be flexible about SetPosition signature
local function setWorldPosition(self, x, y, z)
    -- try SetPosition(x, y, z)
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
        moveSpeed  = 2.0,    -- units per second
        jumpSpeed  = 5.0,    -- initial jump velocity
        gravity    = -9.8,
    },

    Awake = function(self)
        print("[LUA][PlayerMovement] Awake")
        self._velY       = 0.0
        self._groundY    = 0.0
        self._isGrounded = true

        -- debug what GetPosition actually returns
        local a, b, c = self:GetPosition()
        print("[LUA][PlayerMovement] GetPosition raw returns:", a, b, c)
    end,

    Start = function(self)
        print("[LUA][PlayerMovement] Start")

        local pos = getWorldPosition(self)
        print(string.format("[LUA][PlayerMovement] Start position: x=%.3f y=%.3f z=%.3f", pos.x, pos.y, pos.z))

        self._groundY = pos.y

        -- initial broadcast
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
        -- 1) Horizontal input (WASD)
        --------------------------------------
        local moveX, moveZ = 0.0, 0.0

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

        local len = math.sqrt(moveX * moveX + moveZ * moveZ)
        if len > 0.0001 then
            moveX = moveX / len
            moveZ = moveZ / len
        end

        local speed = self.moveSpeed or 5.0
        local dx = moveX * speed * dt
        local dz = moveZ * speed * dt

        --------------------------------------
        -- 2) Jump + simple gravity
        --------------------------------------
        local pos = getWorldPosition(self)

        -- NOTE: if this errors with nil, try Input.Key.SPACE
        if self._isGrounded and Input.GetKeyDown(Input.Key.Space) then
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
            newY          = self._groundY
            self._velY    = 0.0
            self._isGrounded = true
        end

        --------------------------------------
        -- 3) Apply movement to Transform
        --------------------------------------
        local newX = pos.x + dx
        local newZ = pos.z + dz

        setWorldPosition(self, newX, newY, newZ)

        -- broadcast player world position for the camera
        if event_bus and event_bus.publish then
            local p = getWorldPosition(self)
            event_bus.publish("player_position", {
                x = p.x,
                y = p.y,
                z = p.z,
            })
        end
    end,

    OnDisable = function(self)
        local pos = getWorldPosition(self)
        print(string.format(
            "[LUA][PlayerMovement] OnDisable at position: x=%.3f y=%.3f z=%.3f",
            pos.x, pos.y, pos.z
        ))
    end,
}
