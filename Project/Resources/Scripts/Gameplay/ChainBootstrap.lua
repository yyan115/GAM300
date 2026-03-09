-- ChainBootstrap.lua
-- =============================================================================
-- CHAIN CONTROL INTERACTIONS
-- TAP  = press and release before hold threshold
-- HOLD = press, cross hold threshold, then release
-- =============================================================================
--
-- 1. TAP — chain fully retracted (len = 0)
--    Fires chain toward the closest LockOn-tagged entity within LockOnAngleDeg
--    of player forward. Falls back to player forward if none in range.
--
-- 2. TAP — chain mid-extension (actively shooting out)
--    Chain tip drops into flop/physics mode at current position.
--
-- 3. TAP — chain extended, idle, NOT attached
--    Retracts chain.
--
-- 4. TAP — chain extended, idle, ATTACHED (locked/snapped)
--    Retracts chain. If chainLen is near zero (hit on first frame of extension),
--    force-clears the lock and fires endpoint_retracted immediately.
--
-- 5. HOLD — chain fully retracted (len = 0)
--    Activates aim camera while held.
--    On release: fires chain in camera forward direction.
--
-- 6. HOLD — chain extended, idle, ATTACHED
--    While held: chainLen tracks real arc distance player→endpoint each frame,
--    capped at MaxLength. Player can walk closer (chain shortens, links retract)
--    or further (chain lengthens, links activate). Constraint resists movement
--    beyond MaxLength. On release: current length confirmed, chain stays locked.
--
-- 7. HOLD — chain extended, idle, NOT attached
--    [RESERVED — no interaction defined yet]
--
-- =============================================================================
_G.CHAIN_DEBUG = _G.CHAIN_DEBUG ~= nil and _G.CHAIN_DEBUG or false
--_G.CHAIN_DEBUG = true
local function dbg(...) if _G.CHAIN_DEBUG then print(...) end end
local Component = require("extension.mono_helper")
local LinkHandlerModule = require("Gameplay.ChainLinkTransformHandler")
local ControllerModule  = require("Gameplay.ChainController")
local ChainAudioModule  = require("Gameplay.ChainAudio")

return Component {
    fields = {
        NumberOfLinks = 200,
        LinkName = "Link",
        ChainSpeed = 100.0,
        MaxLength = 10.0,
        PlayerName = "Kusane_Player_LeftHandMiddle1",
        VerletGravity = 0.5,
        VerletDamping = 0.02,
        ConstraintIterations = 20,
        IsElastic = true,
        LinkMaxDistance = 0.025,
        PinEndWhenExtended = true,
        AnchorAngleThresholdDeg = 45,
        SubSteps = 4,
        ChainEndpointName = "ChainEndpoint",
        GroundClamp = true,
        GroundClampOffset = 0.1,
        WallClamp = true,
        WallClampInterval = 10,
        WallClampRadius = 0,
        ChainSlackDistance = 1.0,   -- extra metres player can move past chainLen before chain flops
        DragTag = "HeavyEnemy",     -- entity tag that drags the player instead of flopping
        UseLOSAnchors = true,       -- when true: anchors auto-created wherever geometry breaks LOS
        LockOnAngleDeg = 45.0,      -- half-cone: LockOn targets outside this angle from player forward are ignored

        -- =====================================================================
        -- SPIN (aim-hold from retracted state)
        -- Chain spins in a circle on the X axis (YZ plane) while aim is held.
        -- Released when tip points within SpinReleaseAngleTolerance of camera forward.
        -- =====================================================================
        SpinRadius               = 0.75,  -- radius of the circular spin path (metres)
        SpinSpeed                = 25.0,  -- full speed in rad/s (~4 rotations/sec), multiplied by dt so framerate-independent
        SpinWindUpTime           = 0.4,   -- seconds to reach full speed from 0
        SpinVerletBlendTime      = 0.3,   -- seconds to blend from Verlet → scripted positions
        SpinReleaseAngleTolerance= 20.0,  -- degrees: tip must point within this of camera forward to release
        SpinDirection            = 1,     -- 1 = forward→up→back→down, -1 = reverse. Tweak if spin looks wrong.

        -- =====================================================================
        -- AUDIO CHANNELS
        -- ChainAudio resolves AudioComponents from link entities by name at
        -- runtime using Engine.FindAudioCompByName.
        --   Loop : throw / retract one-shots + flop loop
        --   Shot : hit one-shots (flesh or wall) — separate channel so they
        --          never cut off a still-playing throw/retract sound
        --   Aim  : aim-camera loop (2D, reserved — no asset yet)
        -- These are derived from LinkName automatically — no ID needed.
        -- =====================================================================
        AudioVolume = 1.0,

        -- Spatial falloff — tune to match your world scale.
        -- MinDistance: radius at which sound is still full volume.
        -- MaxDistance: beyond this the sound is inaudible.
        -- DopplerLevel: 0=no doppler, 1=realistic. Applied to flop tip
        --               which moves fast through the world.
        AudioMinDistance  = 1.0,
        AudioMaxDistance  = 15.0,
        AudioDopplerLevel = 0.5,

        -- Per-play randomisation to stop sounds feeling stale.
        -- Pitch:   +/- this fraction each trigger  (0.1 = +-10%)
        -- Volume:  +/- this fraction each trigger  (0.08 = +-8%)
        AudioPitchVariation  = 0.1,
        AudioVolumeVariation = 0.08,

        -- =====================================================================
        -- AUDIO CLIPS
        -- Arrays: ChainAudio picks randomly on each trigger.
        -- Add more GUIDs at any time — no code changes needed.
        --
        -- Currently mapped to available assets:
        --   Throw    <- ChainThrow1, ChainThrow2, ChainThrow3, ChainThrow4
        --   Retract  <- ChainRetract1, ChainRetract2, ChainRetract3
        --   HitFlesh <- ChainHitFlesh1, ChainHitFlesh2, ChainHitFlesh3
        --   HitWall  <- ChainHitWall1, ChainHitWall2, ChainHitWall3
        --
        -- Awaiting new assets:
        --   Flop     <- ChainFlop1/2/3     (looping, chain swinging loose)
        --   WallRub  <- ChainWallRub1/2    (looping, chain scraping geometry)
        --   Aim      <- ChainAim1          (looping, helicopter spin overhead)
        --   Taut     <- ChainTaut1/2/3     (one-shot, chain snapping to tension)
        --   Lax      <- ChainLax1/2        (one-shot, tension releasing)
        -- =====================================================================
        AudioClips_Throw    = {},
        AudioClips_Retract  = {},
        AudioClips_HitFlesh = {},
        AudioClips_HitWall  = {},
        AudioClips_Flop     = {},
        AudioClips_WallRub  = {},
        AudioClips_Aim      = {},
        AudioClips_Taut     = {},
        AudioClips_Lax      = {},

        -- HitFlesh plays louder than the global AudioVolume since enemy impact
        -- needs to cut through other sounds clearly.
        -- 1.0 = same as global volume, 1.5 = 50% louder.
        AudioHitFleshVolumeMultiplier = 1.5,
    },

    _unpack_pos = function(self, a, b, c)
        if type(a) == "table" then
            return a[1] or a.x or 0.0, a[2] or a.y or 0.0, a[3] or a.z or 0.0
        end
        return (type(a) == "number") and a or 0.0,
               (type(b) == "number") and b or 0.0,
               (type(c) == "number") and c or 0.0
    end,

    _read_transform_position = function(self, tr)
        if not tr then return 0,0,0 end
        local ok, a, b, c = pcall(function() return tr:GetPosition() end)
        if ok then
            if type(a) == "table" then
                return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
            end
            if type(a) == "number" and type(b) == "number" and type(c) == "number" then
                return a, b, c
            end
        end
        if type(tr.localPosition) == "table" or type(tr.localPosition) == "userdata" then
            local pos = tr.localPosition
            return pos.x or pos[1] or 0, pos.y or pos[2] or 0, pos.z or pos[3] or 0
        end
        return 0,0,0
    end,

    _read_world_pos = function(self, tr)
        if not tr then return 0,0,0 end
        local ok, a, b, c = pcall(function() return Engine.GetTransformWorldPosition and Engine.GetTransformWorldPosition(tr) end)
        if ok and a ~= nil then
            if type(a) == "table" then
                return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
            end
            if type(a) == "number" and type(b) == "number" and type(c) == "number" then
                return a, b, c
            end
        end
        return self:_read_transform_position(tr)
    end,

    _write_world_pos = function(self, tr, x, y, z)
        if not tr then return false end
        if Engine and type(Engine.SetTransformWorldPosition) == "function" then
            local ok = pcall(function() Engine.SetTransformWorldPosition(tr, x, y, z) end)
            if ok then return true end
        end
        if type(tr.SetPosition) == "function" then
            local suc = pcall(function() tr:SetPosition(x, y, z) end)
            if suc then return true end
        end
        if type(tr.localPosition) ~= "nil" then
            pcall(function()
                local pos = tr.localPosition
                if type(pos) == "table" then
                    pos.x, pos.y, pos.z = x, y, z
                    tr.isDirty = true
                elseif type(pos) == "userdata" then
                    pos.x, pos.y, pos.z = x, y, z
                    tr.isDirty = true
                end
            end)
            return true
        end
        return false
    end,

    _on_chain_down = function(self, payload)
        dbg("down")
        self._chain_pressing     = true
        self._chain_held         = false
        self._intentContinue     = false
        self._intentAimFire      = false
        self._intentAdjustLength = false
    end,

    _on_chain_up = function(self, payload)
        dbg("up chain control")
        self._chain_pressing = false

        -- Aim camera off — suppressed if we are entering spin-release mode
        -- (_intentAimFire means we were spinning; _resetSpin will publish it after fire)
        if not self._intentAimFire then
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("chain.aim_camera", {active = false})
            end
        end

        if not self.controller then
            self._chain_held     = false
            self._intentContinue = false
            self._intentAimFire  = false
            return
        end

        local len   = (self.controller.chainLen or 0)
        local isExt = self.controller.isExtending or false
        local isRet = self.controller.isRetracting or false

        dbg(string.format("[ChainBootstrap] _on_chain_up: len=%.4f isExt=%s isRet=%s held=%s intentContinue=%s intentAimFire=%s",
            len, tostring(isExt), tostring(isRet), tostring(self._chain_held),
            tostring(self._intentContinue), tostring(self._intentAimFire)))

        if self._intentContinue then
            -- Update drove ContinueExtension while held. Stop in place on release.
            -- Never retract or re-fire from this branch.
            if isExt then
                self.controller:StopExtension()
                dbg("[ChainBootstrap] ContinueExtension release -> StopExtension")
            end

        elseif self._intentAdjustLength then
            -- Length-adjust mode: player was tuning chainLen while attached.
            -- On release the length is already set — just leave the chain locked as-is.
            dbg(string.format("[ChainBootstrap] AdjustLength release -> confirmed chainLen=%.4f", self.controller.chainLen or 0))

        elseif self._intentAimFire then
            -- Button released during spin — don't fire immediately.
            -- _pendingSpinRelease lets Update keep spinning until the tip
            -- points at camera forward within SpinReleaseAngleTolerance.
            self._pendingSpinRelease = true
            dbg("[ChainBootstrap] SpinRelease pending — waiting for correct angle window")
            -- Held from retracted: fire with camera forward on release.
            local dir = self._cameraForward
            dbg(string.format("[ChainBootstrap] AimFire release -> StartExtension (%.3f,%.3f,%.3f)", dir[1],dir[2],dir[3]))
            self.controller:StartExtension(dir, self.MaxLength, self.LinkMaxDistance)
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("force_player_rotation_to_direction", {x = dir[1], z = dir[3]})
            end

        elseif isExt and not isRet then
            -- Released during extension before hold threshold.
            -- If the endpoint already locked onto something (trigger fired this frame),
            -- skip flop and go straight to retraction so the enemy gets the hooked message.
            local isAttached = self.controller.endPointLocked or self.controller._raycastSnapped
            if isAttached then
                self.controller.isExtending = false
                if self._hookedIsThrowable then
                    -- Throwable hooked mid-extension: stop extending but STAY locked.
                    -- Player taps again to throw or swing — handled in the len>1e-4 branch.
                    dbg("[ChainBootstrap] Tap-release mid-extension: throwable hooked — staying locked, no retract")
                else
                    self.controller:StartRetraction()
                    dbg("[ChainBootstrap] Tap-release mid-extension but already attached -> StartRetraction")
                end
            else
                -- Nothing hit yet: drop into flop so tip falls from wherever it stopped.
                self.controller.isExtending              = false
                self.controller._flopping                = true
                self.controller._justEnteredFlopFromExt  = true
                dbg("[ChainBootstrap] Tap-release mid-extension -> Flop")
            end

        elseif not isExt and not isRet and len <= 1e-4 then
            local isAttached = self.controller.endPointLocked or self.controller._raycastSnapped
            if isAttached then
                -- Chain hit something on the very first frame so chainLen is still near zero.
                -- StartRetraction guards against len=0 so force-clear the lock directly and
                -- publish endpoint_retracted so ChainEndpointController fires enemy_hooked.
                self.controller.endPointLocked   = false
                self.controller._raycastSnapped  = false
                self.controller.hookedTag        = ""
                self.controller.losAnchors       = {}
                self.controller.chainLen         = 0
                if self._wasChainActive and _G.event_bus and _G.event_bus.publish then
                    local sp = self.controller.startPos
                    _G.event_bus.publish("chain.endpoint_retracted", {
                        position = { x = sp[1], y = sp[2], z = sp[3] },
                    })
                    self._wasChainActive = false
                end
                dbg("[ChainBootstrap] TAP on near-zero attached chain -> force clear + endpoint_retracted")
            else
                -- Normal tap on retracted chain: fire.
                self._pendingPlayerForward = nil
                self._pendingTapFire = true
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("request_player_forward", true)
                end
                dbg("[ChainBootstrap] TAP -> requested player_forward_response")
            end

        elseif len > 1e-4 and not isRet and not isExt then
            -- Tap on extended idle chain.
            print(string.format("[ChainBootstrap] TAP extended idle: hookedIsThrowable=%s hookedId=%s len=%.3f",
                tostring(self._hookedIsThrowable), tostring(self._hookedThrowableEntityId), len))
            if self._hookedIsThrowable and self._hookedThrowableEntityId then
                -- Throw direction: throwable toward player
                local ep = self.controller.lockedEndPoint
                local sp = self.controller.startPos
                local tdx = sp[1] - ep[1]
                local tdy = sp[2] - ep[2]
                local tdz = sp[3] - ep[3]
                local tdist = math.sqrt(tdx*tdx + tdy*tdy + tdz*tdz)
                if tdist > 0.01 then
                    tdx, tdy, tdz = tdx/tdist, tdy/tdist, tdz/tdist
                else
                    local cf = self._cameraForward
                    tdx, tdy, tdz = cf[1], cf[2], cf[3]
                end
                print(string.format("[ChainBootstrap] THROW publishing: entityId=%s dir=(%.2f,%.2f,%.2f)",
                    tostring(self._hookedThrowableEntityId), tdx, tdy, tdz))
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.throwable_throw", {
                        entityId = self._hookedThrowableEntityId,
                        dirX     = tdx,
                        dirY     = tdy,
                        dirZ     = tdz,
                    })
                end
                self._hookedIsThrowable       = false
                self._hookedThrowableEntityId = nil
                self.controller:StartRetraction()
                print("[ChainBootstrap] Throwable TAP: THROW -> StartRetraction")
            else
                -- Normal (enemy or no hook): retract.
                self.controller:StartRetraction()
                print("[ChainBootstrap] TAP on extended idle -> StartRetraction (no throwable)")
            end
        end

        self._chain_held         = false
        self._intentContinue     = false
        self._intentAimFire      = false
        self._intentAdjustLength = false
    end,

    _on_chain_hold = function(self, payload)
        dbg("hold")
        -- Hold threshold crossed. Mark held so Update can start ContinueExtension.
        self._chain_held = true

        if not self.controller then return end

        local len        = (self.controller.chainLen or 0)
        local isExt      = self.controller.isExtending or false
        local isAttached = self.controller.endPointLocked or self.controller._raycastSnapped

        -- Only handle aim camera here. ContinueExtension for an extended chain is
        -- driven continuously in Update (every frame while held), not from this event.
        if len <= 1e-4 and not isExt then
            self._intentAimFire = true
            -- Snapshot player body facing NOW — frozen for the entire spin.
            -- Requesting once here; response arrives next frame via _subPlayerForward.
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("request_player_forward", true)
                _G.event_bus.publish("chain.aim_camera", {active = true})
            end
        elseif len > 1e-4 and not isExt and isAttached then
            if self._hookedIsThrowable then
                -- Throwable hooked: never enter length-adjust mode.
                -- Tap-release must reach the throw/swing branch in _on_chain_up.
                dbg("[ChainBootstrap] hold on throwable — skipping AdjustLength, tap-release will throw/swing")
            else
                -- Chain is extended AND attached to something: enter length-adjust mode.
                self._intentAdjustLength = true
                dbg(string.format("[ChainBootstrap] hold on attached chain (len=%.4f) -> AdjustLength mode", len))
            end
        end
    end,

    Start = function(self)
        if self.NumberOfLinks > 0 then
            Engine.CreateEntityDup(self.LinkName, self.LinkName, self.NumberOfLinks)
        end

        self._runtime = self._runtime or {}
        self._runtime.childTransforms = {}
        for i = 1, math.max(1, self.NumberOfLinks) do
            local name = self.LinkName .. tostring(i)
            local tr = Engine.FindTransformByName(name)
            if tr then table.insert(self._runtime.childTransforms, tr) end
        end

        self.linkHandler = LinkHandlerModule.New(self)
        self.linkHandler:InitTransforms(self._runtime.childTransforms)

        local params = {
            NumberOfLinks = self.NumberOfLinks,
            ChainSpeed = self.ChainSpeed,
            MaxLength = self.MaxLength,
            IsElastic = self.IsElastic,
            LinkMaxDistance = self.LinkMaxDistance,
            VerletGravity = self.VerletGravity,
            VerletDamping = self.VerletDamping,
            ConstraintIterations = self.ConstraintIterations,
            PinEndWhenExtended = self.PinEndWhenExtended,
            AnchorAngleThresholdRad = math.rad(self.AnchorAngleThresholdDeg or 45)
        }
        self.controller = ControllerModule.New(params)

        for i, tr in ipairs(self._runtime.childTransforms) do
            local x, y, z
            local ok, a, b, c = pcall(function()
                if Engine and Engine.GetTransformWorldPosition then
                    return Engine.GetTransformWorldPosition(tr)
                end
                return nil
            end)
            if ok and a ~= nil then
                if type(a) == "table" then x,y,z = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                else x,y,z = a,b,c end
            else
                x,y,z = self:_read_world_pos(tr)
            end
            if self.controller.positions[i] then
                self.controller.positions[i][1], self.controller.positions[i][2], self.controller.positions[i][3] = x, y, z
                self.controller.prev[i][1], self.controller.prev[i][2], self.controller.prev[i][3] = x, y, z
            end
        end

        self._cameraForward      = {0, 0, 1}
        self._chain_pressing     = false
        self._chain_held         = false
        self._intentContinue     = false
        self._intentAimFire      = false
        self._intentAdjustLength = false   -- hold on attached chain: live-adjust chainLen to real distance
        self._pendingTapFire     = false
        self._pendingPlayerForward = nil

        -- Spin state
        self._spinTime           = 0
        self._spinAngle          = 0
        self._spinCurrentSpeed   = 0
        self._spinVerletBlend    = 1.0
        self._pendingSpinRelease = false
        self._spinFacingX        = 0
        self._spinFacingZ        = 1
        self._spinFacingLocked   = false  -- true once facing is snapshotted, prevents mid-spin updates

        -- Throwable interaction state
        self._hookedIsThrowable      = false   -- true while endpoint is on a Throwable
        self._hookedThrowableEntityId = nil    -- root entity id of the attached throwable
        self._throwAimThreshold      = 0.3    -- dot-product cutoff for "facing the throwable"

        -- =====================================================================
        -- AUDIO: ChainAudio resolves link AudioComponents by name each frame
        -- =====================================================================
        self.audioHandler = ChainAudioModule.New(
            self.LinkName,
            {
                throw    = self.AudioClips_Throw,
                retract  = self.AudioClips_Retract,
                hitFlesh = self.AudioClips_HitFlesh,
                hitWall  = self.AudioClips_HitWall,
                flop     = self.AudioClips_Flop,
                wallRub  = self.AudioClips_WallRub,
                aim      = self.AudioClips_Aim,
                taut     = self.AudioClips_Taut,
                lax      = self.AudioClips_Lax,
            },
            {
                volume             = self.AudioVolume,
                minDistance        = self.AudioMinDistance,
                maxDistance        = self.AudioMaxDistance,
                dopplerLevel       = self.AudioDopplerLevel,
                pitchVariation     = self.AudioPitchVariation,
                volVariation       = self.AudioVolumeVariation,
                hitFleshVolMult    = self.AudioHitFleshVolumeMultiplier,
            }
        )
        self.audioHandler:Start()

        -- LockOn targets are queried live via Engine.GetEntitiesByTag at fire time.

        self._endpointTransform = nil
        if self.ChainEndpointName and self.ChainEndpointName ~= "" then
            self._endpointTransform = Engine.FindTransformByName(self.ChainEndpointName)
            if not self._endpointTransform then
                dbg("[ChainBootstrap] WARNING: ChainEndpoint object not found: " .. tostring(self.ChainEndpointName))
            end
        end

        if _G.event_bus and _G.event_bus.subscribe then
            self._cameraForwardSub = _G.event_bus.subscribe("ChainAim_basis", function(payload)
                if payload and payload.forward then
                    local fwd = payload.forward
                    local fx = fwd.x or fwd[1] or 0
                    local fy = fwd.y or fwd[2] or 0
                    local fz = fwd.z or fwd[3] or 0
                    local mag = math.sqrt(fx*fx + fy*fy + fz*fz)
                    if mag > 0.0001 then
                        self._cameraForward = {fx/mag, fy/mag, fz/mag}
                    end
                end
            end)

            self._chainSubDown = _G.event_bus.subscribe("chain.down", function(payload) if not payload then return end pcall(function() self:_on_chain_down(payload) end) end)
            self._chainSubUp   = _G.event_bus.subscribe("chain.up",   function(payload) if not payload then return end pcall(function() self:_on_chain_up(payload)   end) end)
            self._chainSubHold = _G.event_bus.subscribe("chain.hold", function(payload) if not payload then return end pcall(function() self:_on_chain_hold(payload) end) end)

            self._subPlayerForward = _G.event_bus.subscribe("player_forward_response", function(payload)
                if not payload then return end
                local x = payload.x
                local z = payload.z
                -- Always update spin facing — continuity correction in _computeSpinPositions
                -- prevents the tip from teleporting when facing changes mid-spin.
                if x and z then
                    local flen = math.sqrt(x*x + z*z)
                    if flen > 1e-6 then
                        self._spinFacingX = x / flen
                        self._spinFacingZ = z / flen
                    end
                end
                -- Tap-fire one-shot use
                if self._pendingPlayerForward then return end
                if not x or not z then return end
                self._pendingPlayerForward = { x, payload.y or 0, z }
            end)

            -- Update lockedEndPoint every frame while hooked so Verlet pins
            -- positions[aN] to the correct moving world position.
            -- ChainEndpointController reads its own parented world transform
            -- and publishes it here — cheap since engine already computed it.
            self._subHookedPos = _G.event_bus.subscribe("chain.endpoint_hooked_position", function(payload)
                if not payload then return end
                pcall(function()
                    if self.controller then
                        self.controller.lockedEndPoint[1] = payload.x
                        self.controller.lockedEndPoint[2] = payload.y
                        self.controller.lockedEndPoint[3] = payload.z
                    end
                end)
            end)

            self._subHitEntity = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(payload)
                if not payload then return end
                pcall(function()
                    if self.controller then
                        dbg("[ChainBootstrap] Endpoint hit entity '" .. tostring(payload.entityName) .. "' — locking endpoint")
                        self.controller.endPointLocked = true
                        self.controller.hookedTag      = payload.rootTag or ""

                        -- NEW: track whether the hooked entity is a throwable
                        self._hookedIsThrowable       = payload.isThrowable or false
                        self._hookedThrowableEntityId = self._hookedIsThrowable
                                                        and payload.rootEntityId
                                                        or nil

                        -- Snapshot lockedEndPoint at moment of hit (unchanged)
                        if self._endpointTransform then
                            local ok, a, b, c = pcall(function()
                                return Engine.GetTransformWorldPosition(self._endpointTransform)
                            end)
                            if ok and a ~= nil then
                                local lx, ly, lz
                                if type(a) == "table" then
                                    lx, ly, lz = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                                elseif type(a) == "number" then
                                    lx, ly, lz = a, b, c
                                end
                                if lx then
                                    self.controller.lockedEndPoint[1] = lx
                                    self.controller.lockedEndPoint[2] = ly
                                    self.controller.lockedEndPoint[3] = lz
                                    dbg(string.format("[ChainBootstrap] lockedEndPoint snapshot at hit: (%.3f,%.3f,%.3f)", lx, ly, lz))

                                    -- For throwables: trim chain to actual player→endpoint distance
                                    -- so the chain doesn't hang loose at full MaxLength.
                                    if self._hookedIsThrowable then
                                        local sp = self.controller.startPos
                                        local ddx = lx - sp[1]
                                        local ddy = ly - sp[2]
                                        local ddz = lz - sp[3]
                                        local actualDist = math.sqrt(ddx*ddx + ddy*ddy + ddz*ddz)
                                        self.controller.chainLen = actualDist
                                        local lmd = tonumber(self.LinkMaxDistance) or 0.025
                                        if lmd > 0 then
                                            self.controller.activeN = math.min(
                                                math.ceil(actualDist / lmd) + 1,
                                                self.controller.n
                                            )
                                        end
                                        dbg(string.format("[ChainBootstrap] Throwable hit — trimmed chainLen=%.3f activeN=%d", actualDist, self.controller.activeN))
                                    end
                                end
                            end
                        end

                        -- Tell the throwable it is now attached so it can start receiving pull
                        if self._hookedIsThrowable and self._hookedThrowableEntityId then
                            _G.event_bus.publish("chain.throwable_attached", {
                                entityId = self._hookedThrowableEntityId,
                            })
                            print(string.format("[ChainBootstrap] throwable_attached published for id=%s", tostring(self._hookedThrowableEntityId)))
                        end
                    end
                end)
            end)
        end
    end,

    -- Returns a normalised direction {x,y,z} toward the closest LockOn target within
    -- the half-cone, or nil if none qualify. pfx/pfz = normalised player forward XZ.
    _pickLockOnDirection = function(self, pfx, pfy, pfz)
        if not Engine or not Engine.GetEntitiesByTag then return nil end

        local halfCos = math.cos(math.rad(tonumber(self.LockOnAngleDeg) or 45.0))
        local sx = self.controller.startPos[1]
        local sy = self.controller.startPos[2]
        local sz = self.controller.startPos[3]

        local ok, entities = pcall(function()
            return Engine.GetEntitiesByTag("LockOn", 32)
        end)
        if not ok or type(entities) ~= "table" then return nil end

        local bestDist = math.huge
        local bestDX, bestDY, bestDZ = nil, nil, nil

        for _, entityId in ipairs(entities) do
            repeat
                local tr = nil
                pcall(function() tr = Engine.FindTransformByID(entityId) end)
                if not tr then break end

                local tx, ty, tz
                local rok, a, b, c = pcall(function()
                    return Engine.GetTransformWorldPosition(tr)
                end)
                if rok and a ~= nil then
                    if type(a) == "table" then tx, ty, tz = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                    elseif type(a) == "number" then tx, ty, tz = a, b, c end
                end
                if not tx then break end

                local dx, dy, dz = tx - sx, ty - sy, tz - sz
                local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
                if dist < 1e-4 then break end

                -- Cone check on XZ plane only
                local flatLen = math.sqrt(dx*dx + dz*dz)
                local dot = flatLen > 1e-4 and ((dx/flatLen)*pfx + (dz/flatLen)*pfz) or 0
                if dot < halfCos then break end

                -- LOS check: raycast from player toward target.
                -- Accept only if nothing is hit before reaching the target (clear LOS),
                -- OR if the first thing hit belongs to the target entity itself.
                if Physics then
                    local ndx, ndy, ndz = dx/dist, dy/dist, dz/dist
                    local hasLOS = true
                    if Physics.RaycastFull then
                        local rok2, hit, hitDist, _, _, _, _, _, _, hitBodyId = pcall(function()
                            return Physics.RaycastFull(sx, sy, sz, ndx, ndy, ndz, dist)
                        end)
                        if rok2 and hit and hitDist and hitDist < dist - 0.1 then
                            hasLOS = (hitBodyId == entityId)
                        end
                    elseif Physics.Raycast then
                        local rok2, hitDist = pcall(function()
                            return Physics.Raycast(sx, sy, sz, ndx, ndy, ndz, dist)
                        end)
                        if rok2 and hitDist and hitDist > 0 and hitDist < dist - 0.1 then
                            hasLOS = false
                        end
                    end
                    if not hasLOS then
                        dbg(string.format("[ChainBootstrap] LockOn entityId=%d blocked by geometry", entityId))
                        break
                    end
                end

                if dist < bestDist then
                    bestDist = dist
                    bestDX, bestDY, bestDZ = dx/dist, dy/dist, dz/dist
                end
            until true
        end

        return bestDX, bestDY, bestDZ
    end,

    -- =========================================================================
    -- SPIN HELPERS
    -- =========================================================================

    -- Advance spin angle and blend weight each frame, returns current angular speed.
    -- SpinSpeed is in rad/s and multiplied by dt — uniform across all framerates.
    _tickSpin = function(self, dt)
        self._spinTime = self._spinTime + dt
        local t = math.min(self._spinTime / math.max(self.SpinWindUpTime, 1e-4), 1.0)
        local dir = (tonumber(self.SpinDirection) or 1) >= 0 and 1 or -1
        local currentSpeed = (tonumber(self.SpinSpeed) or 25.0) * t * dir
        self._spinAngle = self._spinAngle + currentSpeed * dt
        self._spinVerletBlend = math.max(0.0,
            1.0 - self._spinTime / math.max(self.SpinVerletBlendTime, 1e-4))
        -- Request player body facing — response updates _spinFacingX/_spinFacingZ each frame
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("request_player_forward", true)
        end
        return currentSpeed
    end,

    -- Compute scripted circular positions in the player's local forward-up plane.
    -- Rotation axis = player's right (local X), so circle sweeps forward↔up↔back↔down
    -- relative to wherever the player is facing.
    -- Returns positions table AND the tangent direction at the tip (for endpoint rotation).
    -- Spin plane: vertical circle in front of the player, axis = player right (local X).
    -- forward = player body facing (XZ), up = world up (0,1,0).
    -- Circle: cos(a)*forward + sin(a)*up.  Tangent: -sin(a)*forward + cos(a)*up.
    -- SpinFacing is updated each frame by _tickSpin via request_player_forward.
    _computeSpinPositions = function(self, startPos, angle, spinActiveN)
        local result = {}
        local r      = tonumber(self.SpinRadius) or 0.75
        local fx, fz = self._spinFacingX or 0, self._spinFacingZ or 1
        local flen   = math.sqrt(fx*fx + fz*fz)
        if flen < 1e-6 then fx, fz = 0, 1 else fx, fz = fx/flen, fz/flen end

        -- Angle continuity: when facing rotates mid-spin, adjust _spinAngle so the
        -- tip stays at the same world position instead of teleporting.
        local prevFX = self._spinPrevFacingX or fx
        local prevFZ = self._spinPrevFacingZ or fz
        local facingChanged = math.abs(prevFX - fx) > 1e-4 or math.abs(prevFZ - fz) > 1e-4
        if facingChanged and self._spinTime > 0.05 then
            local prevCa = math.cos(angle)
            local prevSa = math.sin(angle)
            local tipFwdComponent = (prevFX * fx + prevFZ * fz) * prevCa
            tipFwdComponent = math.max(-1, math.min(1, tipFwdComponent))
            self._spinAngle = math.atan(prevSa, tipFwdComponent)
        end
        self._spinPrevFacingX = fx
        self._spinPrevFacingZ = fz

        local sa, ca = math.sin(self._spinAngle), math.cos(self._spinAngle)
        for i = 1, spinActiveN do
            local frac = i / spinActiveN
            result[i] = {
                startPos[1] + fx * ca * r * frac,
                startPos[2] + sa * r * frac,
                startPos[3] + fz * ca * r * frac,
            }
        end

        -- Radial direction (startPos to tip) for endpoint rotation
        local tip = result[spinActiveN]
        local dx = tip[1] - startPos[1]
        local dy = tip[2] - startPos[2]
        local dz = tip[3] - startPos[3]
        local dlen = math.sqrt(dx*dx + dy*dy + dz*dz)
        if dlen > 1e-6 then dx,dy,dz = dx/dlen, dy/dlen, dz/dlen end

        return result, dx, dy, dz
    end,

    -- Stop spin, reset all spin state, publish aim camera off
    _resetSpin = function(self)
        self._spinTime           = 0
        self._spinAngle          = 0
        self._spinCurrentSpeed   = 0
        self._spinVerletBlend    = 1.0
        self._pendingSpinRelease = false
        self._spinFacingX        = 0
        self._spinFacingZ        = 1
        self._spinFacingLocked   = false
        self._spinPrevFacingX    = nil
        self._spinPrevFacingZ    = nil
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("chain.aim_camera", {active = false})
        end
    end,

    Update = function(self, dt)
        if not self.controller then return end

        if self._pendingTapFire then
            if self._pendingPlayerForward then
                local pf = self._pendingPlayerForward
                local direction = pf

                -- Check for a LockOn target in the cone around player forward
                local lx, ly, lz = self:_pickLockOnDirection(pf[1], pf[2], pf[3])
                if lx then
                    direction = {lx, ly, lz}
                    dbg(string.format("[ChainBootstrap] TAP -> LockOn target (%.3f,%.3f,%.3f)", lx, ly, lz))
                else
                    dbg(string.format("[ChainBootstrap] TAP -> player forward (%.3f,%.3f,%.3f)", pf[1], pf[2], pf[3]))
                end

                self.controller:StartExtension(direction, self.MaxLength, self.LinkMaxDistance)
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("force_player_rotation_to_direction", {x = direction[1], z = direction[3]})
                end
                self._pendingTapFire = false
                self._pendingPlayerForward = nil
            end
        end

        -- Continuous hold logic -----------------------------------------------
        if self._chain_pressing and self._chain_held then
            local len        = (self.controller.chainLen or 0)
            local isExt      = self.controller.isExtending or false
            local isRet      = self.controller.isRetracting or false
            local isAttached = self.controller.endPointLocked or self.controller._raycastSnapped

            if self._intentAdjustLength then
                -- LENGTH-ADJUST MODE: chain is extended and attached.
                -- Every frame, set chainLen to the real arc-path distance from player
                -- to the locked endpoint (through LOS anchors when present).
                -- This works in BOTH directions:
                --   player moves closer  → chainLen shrinks  → chain goes slack / fewer active links
                --   player moves further → chainLen grows    → more links activate, up to MaxLength
                -- The confirmed length is whatever chainLen is when the button is released.

                local sx = self.controller.startPos[1]
                local sy = self.controller.startPos[2]
                local sz = self.controller.startPos[3]
                local ep = self.controller.lockedEndPoint

                -- Build arc distance through LOS anchors (same path Verlet uses)
                local newLen
                local losA = self.controller.losAnchors
                if losA and #losA > 0 then
                    -- Walk player → anchor[1] → ... → anchor[n] → endpoint
                    local prev = {sx, sy, sz}
                    local arc = 0
                    for _, a in ipairs(losA) do
                        local ddx, ddy, ddz = a[1]-prev[1], a[2]-prev[2], a[3]-prev[3]
                        arc = arc + math.sqrt(ddx*ddx + ddy*ddy + ddz*ddz)
                        prev = a
                    end
                    local ddx, ddy, ddz = ep[1]-prev[1], ep[2]-prev[2], ep[3]-prev[3]
                    arc = arc + math.sqrt(ddx*ddx + ddy*ddy + ddz*ddz)
                    newLen = arc
                else
                    -- No LOS anchors: straight-line to endpoint
                    local ddx, ddy, ddz = ep[1]-sx, ep[2]-sy, ep[3]-sz
                    newLen = math.sqrt(ddx*ddx + ddy*ddy + ddz*ddz)
                end

                newLen = math.min(math.max(newLen, 0), self.MaxLength)
                self.controller.chainLen = newLen

                -- Keep extensionTime consistent with chainLen
                local spd = tonumber(self.ChainSpeed) or 10
                self.controller.extensionTime = newLen / math.max(spd, 1e-6)

                -- Update active link count proportionally so the visual pool
                -- adds or removes links as length changes (mirrors StartExtension logic)
                local lmd = tonumber(self.LinkMaxDistance) or 0
                if lmd > 0 then
                    self.controller.activeN = math.min(
                        math.ceil(newLen / lmd) + 1,
                        self.controller.n
                    )
                end

                dbg(string.format("[ChainBootstrap] AdjustLength: chainLen=%.4f activeN=%d",
                    newLen, self.controller.activeN))

            elseif not self._intentContinue and not self._intentAimFire then
                -- FUTURE: hold on extended, idle, unattached chain.
                dbg("[ChainBootstrap] Hold on free extended chain — no interaction defined yet")
            end

        -- Spin tick: runs every frame while aim-spin is held
            if self._intentAimFire then
                self._spinCurrentSpeed = self:_tickSpin(dt)
            end
        end
        ----------------------------------------------------------------------

        self.playerTransform = Engine.FindTransformByName(self.PlayerName)
        if self.playerTransform then
            local sx, sy, sz = self:_read_world_pos(self.playerTransform)
            self.controller:SetStartPos(sx, sy, sz)
        else
            dbg("Cannot find the bloody player, WHY. YOU NAMED WRONG IS IT OR ENGINE FAILING AGAIN.")
        end

        local settings = {
            ChainSpeed = self.ChainSpeed,
            MaxLength = self.MaxLength,
            IsElastic = self.IsElastic,
            LinkMaxDistance = self.LinkMaxDistance,
            VerletGravity = self.VerletGravity,
            VerletDamping = self.VerletDamping,
            ConstraintIterations = self.ConstraintIterations,
            SubSteps = self.SubSteps,
            GroundClamp = self.GroundClamp,
            GroundClampOffset = self.GroundClampOffset,
            WallClamp = self.WallClamp,
            WallClampInterval = self.WallClampInterval,
            WallClampRadius = self.WallClampRadius,
            ChainSlackDistance = self.ChainSlackDistance,
            DragTag = self.DragTag,
            UseLOSAnchors = self.UseLOSAnchors,
            AnchorAngleThresholdRad = math.rad(self.AnchorAngleThresholdDeg or 45),
            PinEndWhenExtended = self.PinEndWhenExtended,
            getStart = function()
                if not self.playerTransform then
                    self.playerTransform = Engine.FindTransformByName(self.PlayerName)
                end
                if self.playerTransform then
                    return self:_read_world_pos(self.playerTransform)
                end
                local sp = (self.controller and self.controller.startPos) or { self:_unpack_pos(self:GetPosition()) }
                return sp[1], sp[2], sp[3]
            end
        }

        local positions, startPos, endPos = self.controller:Update(dt, settings)
        local activeN = self.controller.activeN

        -- Ensure endpoint transform is cached before spin block needs it
        if not self._endpointTransform and self.ChainEndpointName and self.ChainEndpointName ~= "" then
            self._endpointTransform = Engine.FindTransformByName(self.ChainEndpointName)
        end

        -- =====================================================================
        -- SPIN OVERRIDE
        -- While spinning (held or pending release), blend Verlet positions with
        -- scripted circular positions and check the release angle window.
        -- =====================================================================
        local spinHeld    = self._intentAimFire and self._chain_pressing and self._chain_held
        local spinPending = self._pendingSpinRelease

        if spinHeld or spinPending then
            -- Keep ticking spin during pending release (button already up)
            if spinPending and not spinHeld then
                self._spinCurrentSpeed = self:_tickSpin(dt)
            end

            -- Compute how many links to show during spin
            local lmd = tonumber(self.LinkMaxDistance) or 0.025
            local spinR = tonumber(self.SpinRadius) or 2.0
            local spinActiveN = lmd > 0
                and math.min(math.ceil(spinR / lmd) + 1, self.controller.n)
                or activeN

            -- Scripted circular positions in player-body-forward plane
            local scripted, radX, radY, radZ = self:_computeSpinPositions(startPos, self._spinAngle, spinActiveN)

            -- LOG: print every 30 frames so console isn't flooded
            self._spinLogTimer = (self._spinLogTimer or 0) + 1
            if self._spinLogTimer >= 30 then
                self._spinLogTimer = 0
                print(string.format("[SpinDBG] facingX=%.3f facingZ=%.3f locked=%s | angle=%.2f | tip=(%.2f,%.2f,%.2f) | start=(%.2f,%.2f,%.2f)",
                    self._spinFacingX or 0, self._spinFacingZ or 0,
                    tostring(self._spinFacingLocked),
                    self._spinAngle,
                    scripted[spinActiveN] and scripted[spinActiveN][1] or 0,
                    scripted[spinActiveN] and scripted[spinActiveN][2] or 0,
                    scripted[spinActiveN] and scripted[spinActiveN][3] or 0,
                    startPos[1], startPos[2], startPos[3]))
            end

            -- Blend: spinVerletBlend=1 → full Verlet, =0 → full scripted
            local scriptedWeight = 1.0 - self._spinVerletBlend
            for i = 1, spinActiveN do
                local vp = positions[i] or startPos
                local sp = scripted[i]
                positions[i] = {
                    vp[1] + (sp[1] - vp[1]) * scriptedWeight,
                    vp[2] + (sp[2] - vp[2]) * scriptedWeight,
                    vp[3] + (sp[3] - vp[3]) * scriptedWeight,
                }
            end

            -- Override activeN and endPos so rotations use spin tip
            activeN = spinActiveN
            endPos  = positions[spinActiveN]

            -- Write endpoint transform — position + rotation aligned to circle tangent
            local tip = positions[spinActiveN]
            if self._endpointTransform and tip then
                self:_write_world_pos(self._endpointTransform, tip[1], tip[2], tip[3])

                -- Rotation: point outward from start toward tip (radial, not tangential)
                if tip then
                    local dx = tip[1] - startPos[1]
                    local dy = tip[2] - startPos[2]
                    local dz = tip[3] - startPos[3]
                    local dlen = math.sqrt(dx*dx + dy*dy + dz*dz)
                    if dlen > 1e-6 then
                        dx, dy, dz = dx/dlen, dy/dlen, dz/dlen
                        local ux, uy, uz = 0, 1, 0
                        local dotv = ux*dx + uy*dy + uz*dz
                        local rx = uy*dz - uz*dy
                        local ry = uz*dx - ux*dz
                        local rz = ux*dy - uy*dx
                        local axisLen = math.sqrt(rx*rx + ry*ry + rz*rz)
                        local qw, qx, qy, qz
                        if axisLen < 1e-6 then
                            qw, qx, qy, qz = dotv > 0 and 1 or 0, dotv > 0 and 0 or 1, 0, 0
                        else
                            rx, ry, rz = rx/axisLen, ry/axisLen, rz/axisLen
                            local half = math.acos(math.max(-1, math.min(1, dotv))) * 0.5
                            local s = math.sin(half)
                            qw = math.cos(half)
                            qx, qy, qz = rx*s, ry*s, rz*s
                        end
                        local qlen = math.sqrt(qw*qw + qx*qx + qy*qy + qz*qz)
                        if qlen > 1e-12 then qw,qx,qy,qz = qw/qlen,qx/qlen,qy/qlen,qz/qlen end
                        pcall(function()
                            local rot = self._endpointTransform.localRotation
                            if rot and (type(rot) == "table" or type(rot) == "userdata") then
                                rot.w, rot.x, rot.y, rot.z = qw, qx, qy, qz
                                self._endpointTransform.isDirty = true
                            end
                        end)
                    end
                end

                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.endpoint_moved", {
                        position    = { x = tip[1], y = tip[2], z = tip[3] },
                        isExtending = false,
                    })
                end
            end

            -- Release window check (only when button has been released)
            if spinPending then
                local tolRad = math.rad(tonumber(self.SpinReleaseAngleTolerance) or 20.0)
                -- At high spin speeds the tip can skip past the window in a single frame.
                -- Extend the window by half the frame's angular step to guarantee it
                -- is always caught regardless of framerate or SpinSpeed.
                local step = (self._spinCurrentSpeed or 0) * dt
                local extendedTol = tolRad + step * 0.5

                -- Normalise angle to [-π, π] so 0 = tip pointing at camera forward
                local a = self._spinAngle % (2 * math.pi)
                if a > math.pi then a = a - 2 * math.pi end

                if math.abs(a) <= extendedTol then
                    dbg(string.format("[ChainBootstrap] SpinRelease fired at normAngle=%.3f tolRad=%.3f", a, extendedTol))
                    local cf = self._cameraForward
                    self:_resetSpin()
                    self.controller:StartExtension(cf, self.MaxLength, self.LinkMaxDistance)
                end
            end
        end
        -- =====================================================================

        -- Publish movement constraint — ChainController computed it, Bootstrap owns event_bus
        if self.controller.constraintResult and _G.event_bus and _G.event_bus.publish then
            local cr = self.controller.constraintResult
            dbg(string.format("[ChainBootstrap][CONSTRAINT] publishing ratio=%.3f exceeded=%s drag=%s",
                cr.ratio or 0, tostring(cr.exceeded), tostring(cr.drag)))
            _G.event_bus.publish("chain.movement_constraint", cr)
        end

        -- Throwable tension: always published while throwable is hooked, including during retraction
        if self.controller.throwableTension and _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("chain.throwable_tension", self.controller.throwableTension)
        end

        self.linkHandler:ApplyPositions(positions, activeN)

        local maxStep = (self.RotationMaxStepRadians or (self.RotationMaxStep and math.rad(self.RotationMaxStep))) or math.rad(60)
        self.linkHandler:ApplyRotations(positions, startPos, endPos, maxStep, true, activeN)

        -- === Audio ===
        if self.audioHandler then
            pcall(function()
                self.audioHandler:Update(dt, self.controller:GetPublicState(), positions, activeN)
            end)
        end

        local public = self.controller:GetPublicState()
        self.m_CurrentLength = public.ChainLength
        self.m_IsExtending = public.IsExtending
        self.m_IsRetracting = public.IsRetracting
        self.m_LinkCount = public.LinkCount
        self.m_ActiveLinkCount = public.ActiveLinkCount

        if not self._endpointTransform and self.ChainEndpointName and self.ChainEndpointName ~= "" then
            self._endpointTransform = Engine.FindTransformByName(self.ChainEndpointName)
        end

        if self._endpointTransform then
            local chainIsActive = (self.m_CurrentLength or 0) > 1e-4 or self.m_IsExtending or public.Flopping
            if chainIsActive then
                -- Only skip writes when endPointLocked — engine owns the transform via parenting.
                -- _raycastSnapped still needs position written so endpoint object
                -- matches positions[aN] which ChainController pins to ex/ey/ez.
                if not self.controller.endPointLocked and not public.Flopping then
                    self:_write_world_pos(self._endpointTransform, endPos[1], endPos[2], endPos[3])

                    -- Rotation: only update when not snapped — endpoint is stationary
                    -- at raycast hit point so no need to reorient every frame
                    if not self.controller._raycastSnapped then
                    do
                    -- Rotation: orient endpoint along chain forward direction
                    local fwd = self.controller.lastForward
                    local fx, fy, fz = fwd[1] or 0, fwd[2] or 0, fwd[3] or 1

                    local ux, uy, uz = 0, 1, 0
                    local dot = ux*fx + uy*fy + uz*fz
                    local rx = uy*fz - uz*fy
                    local ry = uz*fx - ux*fz
                    local rz = ux*fy - uy*fx
                    local axisLen = math.sqrt(rx*rx + ry*ry + rz*rz)

                    local qw, qx, qy, qz
                    if axisLen < 1e-6 then
                        if dot > 0 then qw, qx, qy, qz = 1, 0, 0, 0
                        else qw, qx, qy, qz = 0, 1, 0, 0 end
                    else
                        rx, ry, rz = rx/axisLen, ry/axisLen, rz/axisLen
                        local angle = math.acos(math.max(-1, math.min(1, dot)))
                        local halfAngle = angle * 0.5
                        local s = math.sin(halfAngle)
                        qw = math.cos(halfAngle)
                        qx = rx * s
                        qy = ry * s
                        qz = rz * s
                    end

                    local qlen = math.sqrt(qw*qw + qx*qx + qy*qy + qz*qz)
                    if qlen > 1e-12 then
                        qw, qx, qy, qz = qw/qlen, qx/qlen, qy/qlen, qz/qlen
                    else
                        qw, qx, qy, qz = 1, 0, 0, 0
                    end

                    pcall(function()
                        local rot = self._endpointTransform.localRotation
                        if rot and (type(rot) == "table" or type(rot) == "userdata") then
                            rot.w, rot.x, rot.y, rot.z = qw, qx, qy, qz
                            self._endpointTransform.isDirty = true
                        end
                    end)
                    end -- do
                    end -- if not _raycastSnapped
                elseif public.Flopping then
                    -- Flopping: physics owns last link position — write it to endpoint transform
                    local aN = self.controller.activeN
                    local pos = self.controller.positions[aN]
                    if pos then
                        self:_write_world_pos(self._endpointTransform, pos[1], pos[2], pos[3])
                    end
                    -- Rotation: derive from last segment direction
                    local posN   = self.controller.positions[aN]
                    local posN1  = self.controller.positions[math.max(1, aN - 1)]
                    if posN and posN1 then
                        local fx = posN[1] - posN1[1]
                        local fy = posN[2] - posN1[2]
                        local fz = posN[3] - posN1[3]
                        local flen = math.sqrt(fx*fx + fy*fy + fz*fz)
                        if flen > 1e-6 then
                            fx, fy, fz = fx/flen, fy/flen, fz/flen
                            local ux, uy, uz = 0, 1, 0
                            local dot = ux*fx + uy*fy + uz*fz
                            local rx = uy*fz - uz*fy
                            local ry = uz*fx - ux*fz
                            local rz = ux*fy - uy*fx
                            local axisLen = math.sqrt(rx*rx + ry*ry + rz*rz)
                            local qw, qx, qy, qz
                            if axisLen < 1e-6 then
                                if dot > 0 then qw, qx, qy, qz = 1, 0, 0, 0
                                else qw, qx, qy, qz = 0, 1, 0, 0 end
                            else
                                rx, ry, rz = rx/axisLen, ry/axisLen, rz/axisLen
                                local angle = math.acos(math.max(-1, math.min(1, dot)))
                                local half = angle * 0.5
                                local s = math.sin(half)
                                qw = math.cos(half)
                                qx = rx * s; qy = ry * s; qz = rz * s
                            end
                            local qlen = math.sqrt(qw*qw + qx*qx + qy*qy + qz*qz)
                            if qlen > 1e-12 then
                                qw, qx, qy, qz = qw/qlen, qx/qlen, qy/qlen, qz/qlen
                            end
                            pcall(function()
                                local rot = self._endpointTransform.localRotation
                                if rot and (type(rot) == "table" or type(rot) == "userdata") then
                                    rot.w, rot.x, rot.y, rot.z = qw, qx, qy, qz
                                    self._endpointTransform.isDirty = true
                                end
                            end)
                        end
                    end
                end

                -- Always publish endpoint_moved so ChainEndpointController
                -- can manage RB keepalive and trigger window regardless of lock state
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.endpoint_moved", {
                        position = { x = endPos[1], y = endPos[2], z = endPos[3] },
                        isLocked = public.EndPointLocked,
                        chainLength = self.m_CurrentLength,
                        isExtending = self.m_IsExtending,
                        isRetracting = self.m_IsRetracting,
                    })
                end

                self._wasChainActive = true
            else
                -- Chain inactive: drive endpoint back to start
                -- Skip during spin — spin block already wrote the tip position above
                local spinActive = self._intentAimFire or self._pendingSpinRelease
                if not spinActive then
                    local sp = self.controller.startPos
                    self:_write_world_pos(self._endpointTransform, sp[1], sp[2], sp[3])

                if self._wasChainActive then
                        self._wasChainActive = false
                        if _G.event_bus and _G.event_bus.publish then
                            _G.event_bus.publish("chain.endpoint_retracted", {
                                position = { x = sp[1], y = sp[2], z = sp[3] },
                            })
                        end
                    end
                end -- if not spinActive
            end
        end
    end,

    OnDisable = function(self)
        -- === Audio ===
        if self.audioHandler then pcall(function() self.audioHandler:Cleanup() end) end

        -- === Spin cleanup ===
        if self._pendingSpinRelease or self._intentAimFire then
            pcall(function() self:_resetSpin() end)
        end

        self._hookedIsThrowable       = false
        self._hookedThrowableEntityId = nil

        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._cameraForwardSub then pcall(function() _G.event_bus.unsubscribe(self._cameraForwardSub) end) end
            if self._chainSubDown     then pcall(function() _G.event_bus.unsubscribe(self._chainSubDown)     end) end
            if self._chainSubUp       then pcall(function() _G.event_bus.unsubscribe(self._chainSubUp)       end) end
            if self._chainSubHold     then pcall(function() _G.event_bus.unsubscribe(self._chainSubHold)     end) end
            if self._subPlayerForward then pcall(function() _G.event_bus.unsubscribe(self._subPlayerForward) end) end
            if self._subHookedPos         then pcall(function() _G.event_bus.unsubscribe(self._subHookedPos)         end) end
            if self._subHitEntity         then pcall(function() _G.event_bus.unsubscribe(self._subHitEntity)         end) end
        end
    end,

    StartExtension = function(self)
        if self.controller then
            local dir = self._cameraForward
            self.controller:StartExtension(dir, self.MaxLength, self.LinkMaxDistance)
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("force_player_rotation_to_direction", {x = dir[1], z = dir[3]})
            end
        end
    end,
    StartRetraction = function(self) if self.controller then self.controller:StartRetraction() end end,
    StopExtension   = function(self) if self.controller then self.controller:StopExtension()   end end,
    GetChainState   = function(self) return { Length = self.m_CurrentLength, Count = self.m_LinkCount, ActiveCount = self.m_ActiveLinkCount } end
}