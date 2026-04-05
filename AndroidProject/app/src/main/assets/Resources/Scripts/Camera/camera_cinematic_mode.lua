-- Camera/camera_cinematic_mode.lua
-- Cinematic mode: lerps the camera to scripted positions/rotations.
-- Call updateCinematic() each frame; returns true when cinematic is active
-- so the caller can skip normal camera logic.

local M = {}

-- Applies cinematic camera transform for the current frame.
-- Returns true  → cinematic is active, caller should return early.
-- Returns false → cinematic is inactive, clear start state and continue normally.
function M.updateCinematic(self, dt)
    if not (self._cinematicActive and self._cinematicTarget) then
        -- Clear cached start pose when not in cinematic
        self._cinematicStartPosX = nil
        self._cinematicStartRotW = nil
        return false
    end

    local target = self._cinematicTarget
    local pos    = target.position
    local rot    = target.rotation
    local lerpT  = target.lerpT or 1.0
    local phase  = target.phase or "transition"

    if not pos then return true end

    -- Capture starting pose on the very first frame of a cinematic cut
    if not self._cinematicStartPosX then
        local px, py, pz = self:GetPosition()
        if type(px) == "table" then
            self._cinematicStartPosX = px.x or 0.0
            self._cinematicStartPosY = px.y or 0.0
            self._cinematicStartPosZ = px.z or 0.0
        else
            self._cinematicStartPosX = px or 0.0
            self._cinematicStartPosY = py or 0.0
            self._cinematicStartPosZ = pz or 0.0
        end

        local rw, rx, ry, rz = self:GetRotation()
        self._cinematicStartRotW = rw or 1
        self._cinematicStartRotX = rx or 0
        self._cinematicStartRotY = ry or 0
        self._cinematicStartRotZ = rz or 0
    end

    -- Smooth-step easing on lerpT
    local st = lerpT * lerpT * (3.0 - 2.0 * lerpT)

    if phase == "transition" then
        -- Lerp position from start to target
        local sx = self._cinematicStartPosX
        local sy = self._cinematicStartPosY
        local sz = self._cinematicStartPosZ
        self:SetPosition(
            sx + (pos.x - sx) * st,
            sy + (pos.y - sy) * st,
            sz + (pos.z - sz) * st
        )

        -- Slerp-approximation for rotation (linear + renormalize)
        if rot then
            local tw, tx, ty, tz = rot.qw or 1, rot.qx or 0, rot.qy or 0, rot.qz or 0
            local sw = self._cinematicStartRotW
            local sx2 = self._cinematicStartRotX
            local sy2 = self._cinematicStartRotY
            local sz2 = self._cinematicStartRotZ

            -- Ensure shortest arc
            if sw*tw + sx2*tx + sy2*ty + sz2*tz < 0 then
                tw, tx, ty, tz = -tw, -tx, -ty, -tz
            end

            local nw = sw + (tw - sw) * st
            local nx = sx2 + (tx - sx2) * st
            local ny = sy2 + (ty - sy2) * st
            local nz = sz2 + (tz - sz2) * st
            local invLen = 1.0 / math.sqrt(nw*nw + nx*nx + ny*ny + nz*nz + 0.0001)
            self:SetRotation(nw*invLen, nx*invLen, ny*invLen, nz*invLen)
        end
    else
        -- "stay" phase: hold at the target pose exactly
        self:SetPosition(pos.x, pos.y, pos.z)
        if rot then
            local tw, tx, ty, tz = rot.qw or 1, rot.qx or 0, rot.qy or 0, rot.qz or 0
            local len = math.sqrt(tw*tw + tx*tx + ty*ty + tz*tz + 0.0001)
            self:SetRotation(tw/len, tx/len, ty/len, tz/len)
        end
    end

    self.isDirty = true
    return true -- cinematic is active
end

return M
