// LuaComponentBindings.inc
// Only BEGIN_COMPONENT / PROPERTY / END_COMPONENT lines in this file.
// Edit this file to add/remove properties. Nothing else.

// ============================================================================
// Value-like types (Math types)
// ============================================================================

BEGIN_COMPONENT(Vector3D, "Vector3D")
    PROPERTY("x", &Vector3D::x)
    PROPERTY("y", &Vector3D::y)
    PROPERTY("z", &Vector3D::z)
END_COMPONENT()

BEGIN_COMPONENT(Quaternion, "Quaternion")
    PROPERTY("w", &Quaternion::w)
    PROPERTY("x", &Quaternion::x)
    PROPERTY("y", &Quaternion::y)
    PROPERTY("z", &Quaternion::z)
END_COMPONENT()

// ============================================================================
// Core ECS Components
// ============================================================================

BEGIN_COMPONENT(Transform, "Transform")
    PROPERTY("localPosition", &Transform::localPosition)
    PROPERTY("localScale", &Transform::localScale)
    PROPERTY("localRotation", &Transform::localRotation)
    PROPERTY("isDirty", &Transform::isDirty)
END_COMPONENT()

BEGIN_COMPONENT(NameComponent, "NameComponent")
    PROPERTY("name", &NameComponent::name)
END_COMPONENT()

// ============================================================================
// Animation Components
// ============================================================================

BEGIN_COMPONENT(AnimationComponent, "AnimationComponent")
    PROPERTY("enabled", &AnimationComponent::enabled)
    PROPERTY("isPlay", &AnimationComponent::isPlay)
    PROPERTY("isLoop", &AnimationComponent::isLoop)
    PROPERTY("speed", &AnimationComponent::speed)
    PROPERTY("clipCount", &AnimationComponent::clipCount)
END_COMPONENT()

// ============================================================================
// AI/Brain Components
// ============================================================================

BEGIN_COMPONENT(BrainComponent, "BrainComponent")
    PROPERTY("kindInt", &BrainComponent::kindInt)
    PROPERTY("started", &BrainComponent::started)
    PROPERTY("activeState", &BrainComponent::activeState)
    PROPERTY("enabled", &BrainComponent::enabled)
END_COMPONENT()

// ============================================================================
// Camera Component
// ============================================================================

BEGIN_COMPONENT(CameraComponent, "CameraComponent")
    PROPERTY("enabled", &CameraComponent::enabled)
    PROPERTY("isActive", &CameraComponent::isActive)
    PROPERTY("priority", &CameraComponent::priority)
    PROPERTY("target", &CameraComponent::target)
    PROPERTY("up", &CameraComponent::up)
    PROPERTY("yaw", &CameraComponent::yaw)
    PROPERTY("pitch", &CameraComponent::pitch)
    PROPERTY("useFreeRotation", &CameraComponent::useFreeRotation)
    PROPERTY("fov", &CameraComponent::fov)
    PROPERTY("nearPlane", &CameraComponent::nearPlane)
    PROPERTY("farPlane", &CameraComponent::farPlane)
    PROPERTY("orthoSize", &CameraComponent::orthoSize)
    PROPERTY("movementSpeed", &CameraComponent::movementSpeed)
    PROPERTY("mouseSensitivity", &CameraComponent::mouseSensitivity)
    PROPERTY("minZoom", &CameraComponent::minZoom)
    PROPERTY("maxZoom", &CameraComponent::maxZoom)
    PROPERTY("zoomSpeed", &CameraComponent::zoomSpeed)
    PROPERTY("isShaking", &CameraComponent::isShaking)
    PROPERTY("shakeIntensity", &CameraComponent::shakeIntensity)
    PROPERTY("shakeDuration", &CameraComponent::shakeDuration)
    PROPERTY("shakeTimer", &CameraComponent::shakeTimer)
    PROPERTY("shakeFrequency", &CameraComponent::shakeFrequency)
    PROPERTY("shakeOffset", &CameraComponent::shakeOffset)
    PROPERTY("skyboxTexturePath", &CameraComponent::skyboxTexturePath)
END_COMPONENT()

// ============================================================================
// Light Components
// ============================================================================

BEGIN_COMPONENT(LightComponent, "LightComponent")
    PROPERTY("color", &LightComponent::color)
    PROPERTY("intensity", &LightComponent::intensity)
    PROPERTY("enabled", &LightComponent::enabled)
END_COMPONENT()

BEGIN_COMPONENT(DirectionalLightComponent, "DirectionalLightComponent")
    PROPERTY("color", &DirectionalLightComponent::color)
    PROPERTY("intensity", &DirectionalLightComponent::intensity)
    PROPERTY("enabled", &DirectionalLightComponent::enabled)
    PROPERTY("direction", &DirectionalLightComponent::direction)
    PROPERTY("ambient", &DirectionalLightComponent::ambient)
    PROPERTY("diffuse", &DirectionalLightComponent::diffuse)
    PROPERTY("specular", &DirectionalLightComponent::specular)
END_COMPONENT()

BEGIN_COMPONENT(PointLightComponent, "PointLightComponent")
    PROPERTY("color", &PointLightComponent::color)
    PROPERTY("intensity", &PointLightComponent::intensity)
    PROPERTY("enabled", &PointLightComponent::enabled)
    PROPERTY("constant", &PointLightComponent::constant)
    PROPERTY("linear", &PointLightComponent::linear)
    PROPERTY("quadratic", &PointLightComponent::quadratic)
    PROPERTY("ambient", &PointLightComponent::ambient)
    PROPERTY("diffuse", &PointLightComponent::diffuse)
    PROPERTY("specular", &PointLightComponent::specular)
END_COMPONENT()

BEGIN_COMPONENT(SpotLightComponent, "SpotLightComponent")
    PROPERTY("color", &SpotLightComponent::color)
    PROPERTY("intensity", &SpotLightComponent::intensity)
    PROPERTY("enabled", &SpotLightComponent::enabled)
    PROPERTY("direction", &SpotLightComponent::direction)
    PROPERTY("cutOff", &SpotLightComponent::cutOff)
    PROPERTY("outerCutOff", &SpotLightComponent::outerCutOff)
    PROPERTY("constant", &SpotLightComponent::constant)
    PROPERTY("linear", &SpotLightComponent::linear)
    PROPERTY("quadratic", &SpotLightComponent::quadratic)
    PROPERTY("ambient", &SpotLightComponent::ambient)
    PROPERTY("diffuse", &SpotLightComponent::diffuse)
    PROPERTY("specular", &SpotLightComponent::specular)
END_COMPONENT()

// ============================================================================
// Rendering Components
// ============================================================================

BEGIN_COMPONENT(ParticleComponent, "ParticleComponent")
    PROPERTY("emitterPosition", &ParticleComponent::emitterPosition)
    PROPERTY("emissionRate", &ParticleComponent::emissionRate)
    PROPERTY("maxParticles", &ParticleComponent::maxParticles)
    PROPERTY("particleLifetime", &ParticleComponent::particleLifetime)
    PROPERTY("startSize", &ParticleComponent::startSize)
    PROPERTY("endSize", &ParticleComponent::endSize)
    PROPERTY("startColor", &ParticleComponent::startColor)
    PROPERTY("startColorAlpha", &ParticleComponent::startColorAlpha)
    PROPERTY("endColor", &ParticleComponent::endColor)
    PROPERTY("endColorAlpha", &ParticleComponent::endColorAlpha)
    PROPERTY("gravity", &ParticleComponent::gravity)
    PROPERTY("velocityRandomness", &ParticleComponent::velocityRandomness)
    PROPERTY("initialVelocity", &ParticleComponent::initialVelocity)
    PROPERTY("texturePath", &ParticleComponent::texturePath)
    PROPERTY("timeSinceEmission", &ParticleComponent::timeSinceEmission)
    PROPERTY("isEmitting", &ParticleComponent::isEmitting)
    PROPERTY("isPlayingInEditor", &ParticleComponent::isPlayingInEditor)
    PROPERTY("isPausedInEditor", &ParticleComponent::isPausedInEditor)
END_COMPONENT()

BEGIN_COMPONENT(SpriteRenderComponent, "SpriteRenderComponent")
    PROPERTY("position", &SpriteRenderComponent::position)
    PROPERTY("scale", &SpriteRenderComponent::scale)
    PROPERTY("rotation", &SpriteRenderComponent::rotation)
    PROPERTY("color", &SpriteRenderComponent::color)
    PROPERTY("alpha", &SpriteRenderComponent::alpha)
    PROPERTY("is3D", &SpriteRenderComponent::is3D)
    PROPERTY("enableBillboard", &SpriteRenderComponent::enableBillboard)
    PROPERTY("sortingLayer", &SpriteRenderComponent::sortingLayer)
    PROPERTY("sortingOrder", &SpriteRenderComponent::sortingOrder)
    PROPERTY("saved3DPosition", &SpriteRenderComponent::saved3DPosition)
    PROPERTY("texturePath", &SpriteRenderComponent::texturePath)
    PROPERTY("hasMigratedToTransform", &SpriteRenderComponent::hasMigratedToTransform)
END_COMPONENT()

BEGIN_COMPONENT(TextRenderComponent, "TextRenderComponent")
    PROPERTY("text", &TextRenderComponent::text)
    PROPERTY("fontSize", &TextRenderComponent::fontSize)
    PROPERTY("position", &TextRenderComponent::position)
    PROPERTY("color", &TextRenderComponent::color)
    PROPERTY("is3D", &TextRenderComponent::is3D)
    PROPERTY("sortingLayer", &TextRenderComponent::sortingLayer)
    PROPERTY("sortingOrder", &TextRenderComponent::sortingOrder)
    PROPERTY("alignmentInt", &TextRenderComponent::alignmentInt)
END_COMPONENT()

// ============================================================================
// Physics Components
// ============================================================================

BEGIN_COMPONENT(ColliderComponent, "ColliderComponent")
    PROPERTY("enabled", &ColliderComponent::enabled)
    PROPERTY("layerID", &ColliderComponent::layerID)
    PROPERTY("version", &ColliderComponent::version)
    PROPERTY("shapeTypeID", &ColliderComponent::shapeTypeID)
    PROPERTY("boxHalfExtents", &ColliderComponent::boxHalfExtents)
    PROPERTY("sphereRadius", &ColliderComponent::sphereRadius)
    PROPERTY("capsuleRadius", &ColliderComponent::capsuleRadius)
    PROPERTY("capsuleHalfHeight", &ColliderComponent::capsuleHalfHeight)
    PROPERTY("cylinderRadius", &ColliderComponent::cylinderRadius)
    PROPERTY("cylinderHalfHeight", &ColliderComponent::cylinderHalfHeight)
END_COMPONENT()

BEGIN_COMPONENT(RigidBodyComponent, "RigidBodyComponent")
    PROPERTY("enabled", &RigidBodyComponent::enabled)
    PROPERTY("motionID", &RigidBodyComponent::motionID)
    PROPERTY("ccd", &RigidBodyComponent::ccd)
    PROPERTY("isTrigger", &RigidBodyComponent::isTrigger)
    PROPERTY("gravityFactor", &RigidBodyComponent::gravityFactor)
    PROPERTY("angularVel", &RigidBodyComponent::angularVel)
    PROPERTY("linearVel", &RigidBodyComponent::linearVel)
    PROPERTY("linearDamping", &RigidBodyComponent::linearDamping)
    PROPERTY("angularDamping", &RigidBodyComponent::angularDamping)
    PROPERTY("transform_dirty", &RigidBodyComponent::transform_dirty)
    PROPERTY("motion_dirty", &RigidBodyComponent::motion_dirty)
    PROPERTY("collider_seen_version", &RigidBodyComponent::collider_seen_version)
END_COMPONENT()

// ============================================================================
// Audio Components
// ============================================================================

BEGIN_COMPONENT(AudioComponent, "AudioComponent")
    PROPERTY("enabled", &AudioComponent::enabled)
    PROPERTY("Mute", &AudioComponent::Mute)
    PROPERTY("bypassListenerEffects", &AudioComponent::bypassListenerEffects)
    PROPERTY("PlayOnAwake", &AudioComponent::PlayOnAwake)
    PROPERTY("Loop", &AudioComponent::Loop)
    PROPERTY("Priority", &AudioComponent::Priority)
    PROPERTY("Volume", &AudioComponent::Volume)
    PROPERTY("Pitch", &AudioComponent::Pitch)
    PROPERTY("StereoPan", &AudioComponent::StereoPan)
    PROPERTY("reverbZoneMix", &AudioComponent::reverbZoneMix)
    PROPERTY("Spatialize", &AudioComponent::Spatialize)
    PROPERTY("SpatialBlend", &AudioComponent::SpatialBlend)
    PROPERTY("DopplerLevel", &AudioComponent::DopplerLevel)
    PROPERTY("MinDistance", &AudioComponent::MinDistance)
    PROPERTY("MaxDistance", &AudioComponent::MaxDistance)
    PROPERTY("OutputAudioMixerGroup", &AudioComponent::OutputAudioMixerGroup)
    PROPERTY("IsPlaying", &AudioComponent::IsPlaying)
    PROPERTY("IsPaused", &AudioComponent::IsPaused)
    PROPERTY("Position", &AudioComponent::Position)

    METHOD("play", &AudioComponent::Play)
    METHOD("playOneShot", &AudioComponent::PlayOneShot)
    METHOD("stop", &AudioComponent::Stop)
    METHOD("pause", &AudioComponent::Pause)
    METHOD("unPause", &AudioComponent::UnPause)
    METHOD("getIsPlaying", &AudioComponent::GetIsPlaying)
    METHOD("getIsPaused", &AudioComponent::GetIsPaused)
    METHOD("setVolume", &AudioComponent::SetVolume)
    METHOD("setPitch", &AudioComponent::SetPitch)
    METHOD("setLoop", &AudioComponent::SetLoop)
    METHOD("setMute", &AudioComponent::SetMute)
    METHOD("setClip", &AudioComponent::SetClipFromString)

END_COMPONENT()

BEGIN_COMPONENT(AudioListenerComponent, "AudioListenerComponent")
    PROPERTY("enabled", &AudioListenerComponent::enabled)
END_COMPONENT()

BEGIN_COMPONENT(AudioReverbZoneComponent, "AudioReverbZoneComponent")
    PROPERTY("enabled", &AudioReverbZoneComponent::enabled)
    PROPERTY("MinDistance", &AudioReverbZoneComponent::MinDistance)
    PROPERTY("MaxDistance", &AudioReverbZoneComponent::MaxDistance)
    PROPERTY("reverbPresetIndex", &AudioReverbZoneComponent::reverbPresetIndex)
    PROPERTY("decayTime", &AudioReverbZoneComponent::decayTime)
    PROPERTY("earlyDelay", &AudioReverbZoneComponent::earlyDelay)
    PROPERTY("lateDelay", &AudioReverbZoneComponent::lateDelay)
    PROPERTY("hfReference", &AudioReverbZoneComponent::hfReference)
    PROPERTY("hfDecayRatio", &AudioReverbZoneComponent::hfDecayRatio)
    PROPERTY("diffusion", &AudioReverbZoneComponent::diffusion)
    PROPERTY("density", &AudioReverbZoneComponent::density)
    PROPERTY("lowShelfFrequency", &AudioReverbZoneComponent::lowShelfFrequency)
    PROPERTY("lowShelfGain", &AudioReverbZoneComponent::lowShelfGain)
    PROPERTY("highCut", &AudioReverbZoneComponent::highCut)
    PROPERTY("earlyLateMix", &AudioReverbZoneComponent::earlyLateMix)
    PROPERTY("wetLevel", &AudioReverbZoneComponent::wetLevel)
    PROPERTY("Position", &AudioReverbZoneComponent::Position)
END_COMPONENT()

// ============================================================================
// Tag and Layer Components
// ============================================================================

BEGIN_COMPONENT(TagComponent, "TagComponent")
    PROPERTY("tagIndex", &TagComponent::tagIndex)
END_COMPONENT()

BEGIN_COMPONENT(LayerComponent, "LayerComponent")
    PROPERTY("layerIndex", &LayerComponent::layerIndex)
END_COMPONENT()

// Add more BEGIN_COMPONENT(...) ... PROPERTY(...) ... END_COMPONENT() blocks here.
