#pragma once
#include "ECS/System.hpp"
#include "UI/Slider/SliderComponent.hpp"
#include "Math/Vector3D.hpp"

class ECSManager;
using Entity = unsigned int;

class SliderSystem : public System {
public:
    SliderSystem() = default;
    ~SliderSystem() = default;

    void Initialise(ECSManager& ecsManager);
    void Update();  // Only runs during play mode
    void Shutdown();

private:
    Vector3D GetMousePosInGameSpace() const;
    void EnsureChildEntities(Entity sliderEntity, SliderComponent& sliderComp);
    bool TryResolveEntity(const GUID_128& guid, Entity& outEntity) const;
    void AttachChild(Entity parent, Entity child);
    bool IsMouseOverEntity(Entity entity, const Vector3D& mousePos) const;
    bool UpdateValueFromMouse(Entity sliderEntity, SliderComponent& sliderComp, const Vector3D& mousePos);
    void UpdateHandleFromValue(Entity sliderEntity, SliderComponent& sliderComp);
    void InvokeOnValueChanged(Entity sliderEntity, SliderComponent& sliderComp, float oldValue);

    ECSManager* m_ecs = nullptr;
    Entity m_activeSlider = static_cast<Entity>(-1);
};
