-- DistanceFade.lua
-- Fades out a 3D model when it gets too close to the camera.
-- Attach to any entity with a ModelRenderComponent.
--
-- Usage:
--   FadeNearDistance  = distance where object becomes fully transparent
--   FadeFarDistance   = distance where object is fully opaque
--   FadeMinOpacity    = minimum opacity when fully faded (0 = invisible)

local Component = require("extension.mono_helper")

return Component {
    fields = {
        FadeNearDistance = 2.0,
        FadeFarDistance  = 5.0,
        FadeMinOpacity  = 0.0,
    },

    Start = function(self)
        self._entityId = self:GetEntityId()
    end,

    Update = function(self, dt)
        if not self._entityId then return end

        local ex, ey, ez = Engine.GetEntityPosition(self._entityId)
        if not ex then return end

        -- Camera position
        local rx, ry, rz
        if _G.CAMERA_POS then
            rx = _G.CAMERA_POS.x
            ry = _G.CAMERA_POS.y
            rz = _G.CAMERA_POS.z
        else
            -- Fall back to player
            if not self._playerEntity then
                local players = Engine.GetEntitiesByTag("Player", 1)
                if players and #players > 0 then
                    self._playerEntity = players[1]
                end
            end
            if self._playerEntity then
                rx, ry, rz = Engine.GetEntityPosition(self._playerEntity)
            end
        end

        if not rx then return end

        local dx = ex - rx
        local dy = ey - ry
        local dz = ez - rz
        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)

        -- Close = transparent, far = opaque
        local t = 0.0
        if self.FadeFarDistance > self.FadeNearDistance then
            t = (dist - self.FadeNearDistance) / (self.FadeFarDistance - self.FadeNearDistance)
            t = math.max(0.0, math.min(1.0, t))
        end

        local opacity = self.FadeMinOpacity + t * (1.0 - self.FadeMinOpacity)
        Engine.SetModelOpacity(self._entityId, opacity)
    end,
}
