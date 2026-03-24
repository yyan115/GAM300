require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
    },
    Start = function(self)
        if _G.event_bus then
            -- CHECK ACTIVATED CHECKPOINT EVENT
            _G.event_bus.subscribe("picked_up_weapon", function(isPickedUp)
                if isPickedUp then
                    self:DisableWeaponPickUpVFX()
                end
            end)
        else
            print("event_bus NOT FOUND")
        end
    end,

    DisableWeaponPickUpVFX = function(self)
        if Engine and Engine.GetChildrenEntities then
            local childrenIds = Engine.GetChildrenEntities(self.entityId)
            for _, childId in ipairs(childrenIds) do
                -- 1. Try to get the ParticleComponent for each child
                local particleComp = GetComponent(childId, "ParticleComponent")
                
                if particleComp then
                    particleComp.particleLifetime = 1.5     --Set all lifetime to 1.5, ensure disappear before camera cuts back
                    particleComp.isEmitting = false
                end         
            end    
        end   
    end,
}