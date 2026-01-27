// LuaComponentBindings.inc
// Only BEGIN_COMPONENT / PROPERTY / METHOD / END_COMPONENT lines in this file.
// Edit this file to add/remove properties. Nothing else.

// ============================================================================
// Value-like types (Math types)
// ============================================================================

BEGIN_COMPONENT(Vector2D, "Vector2D")
    PROPERTY("x", &Vector2D::x)
    PROPERTY("y", &Vector2D::y)
END_COMPONENT()

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
    PROPERTY("worldPosition", &Transform::worldPosition)
    PROPERTY("worldRotation", &Transform::worldRotation)
    PROPERTY("worldScale", &Transform::worldScale)
    PROPERTY("isDirty", &Transform::isDirty)
END_COMPONENT()

BEGIN_COMPONENT(NameComponent, "NameComponent")
    PROPERTY("name", &NameComponent::name)
END_COMPONENT()

BEGIN_COMPONENT(ModelRenderComponent, "ModelRenderComponent")
    PROPERTY("isVisible", &ModelRenderComponent::isVisible)
    METHOD("SetVisible", &ModelRenderComponent::SetVisible)
END_COMPONENT()

// ============================================================================
// Animation Components
// ============================================================================

BEGIN_COMPONENT(AnimationComponent, "AnimationComponent")
    // Properties
    PROPERTY("enabled", &AnimationComponent::enabled)
    PROPERTY("isPlay", &AnimationComponent::isPlay)
    PROPERTY("isLoop", &AnimationComponent::isLoop)
    PROPERTY("speed", &AnimationComponent::speed)
    PROPERTY("clipCount", &AnimationComponent::clipCount)
    // Methods - Playback control
    METHOD("Play", &AnimationComponent::Play)
    METHOD("Pause", &AnimationComponent::Pause)
    METHOD("Stop", &AnimationComponent::Stop)
    METHOD("SetLooping", &AnimationComponent::SetLooping)
    METHOD("SetSpeed", &AnimationComponent::SetSpeed)
    METHOD("SetClip", &AnimationComponent::SetClip)
    METHOD("GetActiveClipIndex", &AnimationComponent::GetActiveClipIndex)
    METHOD("PlayClip", &AnimationComponent::PlayClip)
    METHOD("PlayOnce", &AnimationComponent::PlayOnce)
    METHOD("IsPlaying", &AnimationComponent::IsPlaying)
    METHOD("ResetForPlay", &AnimationComponent::ResetForPlay)
    // State Machine parameter control (for animator controller)
    METHOD("SetBool", &AnimationComponent::SetBool)
    METHOD("SetInt", &AnimationComponent::SetInt)
    METHOD("SetFloat", &AnimationComponent::SetFloat)
    METHOD("SetTrigger", &AnimationComponent::SetTrigger)
    METHOD("GetBool", &AnimationComponent::GetBool)
    METHOD("GetInt", &AnimationComponent::GetInt)
    METHOD("GetFloat", &AnimationComponent::GetFloat)
    METHOD("GetCurrentState", &AnimationComponent::GetCurrentState)
END_COMPONENT()

// ============================================================================
// Sprite Animation Component
// ============================================================================

BEGIN_COMPONENT(SpriteAnimationComponent, "SpriteAnimationComponent")
    // Properties
    PROPERTY("currentClipIndex", &SpriteAnimationComponent::currentClipIndex)
    PROPERTY("currentFrameIndex", &SpriteAnimationComponent::currentFrameIndex)
    PROPERTY("playbackSpeed", &SpriteAnimationComponent::playbackSpeed)
    PROPERTY("playing", &SpriteAnimationComponent::playing)
    PROPERTY("enabled", &SpriteAnimationComponent::enabled)
    PROPERTY("autoPlay", &SpriteAnimationComponent::autoPlay)
    // Methods
    METHOD("Play", &SpriteAnimationComponent::Play)
    METHOD("Stop", &SpriteAnimationComponent::Stop)
    METHOD("Resume", &SpriteAnimationComponent::Resume)
END_COMPONENT()

// ============================================================================
// Sprite Animation Component
// ============================================================================

BEGIN_COMPONENT(SpriteRenderComponent, "SpriteRenderComponent")
    // Properties
    PROPERTY("isVisible", &SpriteRenderComponent::isVisible)
    PROPERTY("position", &SpriteRenderComponent::position)
    PROPERTY("scale", &SpriteRenderComponent::scale)
    PROPERTY("rotation", &SpriteRenderComponent::rotation)
    PROPERTY("color", &SpriteRenderComponent::color)
    PROPERTY("alpha", &SpriteRenderComponent::alpha)
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
    // Core properties
    PROPERTY("enabled", &CameraComponent::enabled)
    PROPERTY("isActive", &CameraComponent::isActive)
    PROPERTY("priority", &CameraComponent::priority)
    // Orientation
    PROPERTY("target", &CameraComponent::target)
    PROPERTY("up", &CameraComponent::up)
    PROPERTY("yaw", &CameraComponent::yaw)
    PROPERTY("pitch", &CameraComponent::pitch)
    PROPERTY("useFreeRotation", &CameraComponent::useFreeRotation)
    // Projection settings
    PROPERTY("fov", &CameraComponent::fov)
    PROPERTY("nearPlane", &CameraComponent::nearPlane)
    PROPERTY("farPlane", &CameraComponent::farPlane)
    PROPERTY("orthoSize", &CameraComponent::orthoSize)
    // Movement
    PROPERTY("movementSpeed", &CameraComponent::movementSpeed)
    PROPERTY("mouseSensitivity", &CameraComponent::mouseSensitivity)
    // Zoom
    PROPERTY("minZoom", &CameraComponent::minZoom)
    PROPERTY("maxZoom", &CameraComponent::maxZoom)
    PROPERTY("zoomSpeed", &CameraComponent::zoomSpeed)
    // Camera shake
    PROPERTY("isShaking", &CameraComponent::isShaking)
    PROPERTY("shakeIntensity", &CameraComponent::shakeIntensity)
    PROPERTY("shakeDuration", &CameraComponent::shakeDuration)
    PROPERTY("shakeTimer", &CameraComponent::shakeTimer)
    PROPERTY("shakeFrequency", &CameraComponent::shakeFrequency)
    PROPERTY("shakeOffset", &CameraComponent::shakeOffset)
    // Skybox
    PROPERTY("skyboxTexturePath", &CameraComponent::skyboxTexturePath)
    PROPERTY("useSkybox", &CameraComponent::useSkybox)
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
    // Emitter settings
    PROPERTY("emitterPosition", &ParticleComponent::emitterPosition)
    PROPERTY("emissionRate", &ParticleComponent::emissionRate)
    PROPERTY("maxParticles", &ParticleComponent::maxParticles)
    // Particle settings
    PROPERTY("particleLifetime", &ParticleComponent::particleLifetime)
    PROPERTY("startSize", &ParticleComponent::startSize)
    PROPERTY("endSize", &ParticleComponent::endSize)
    PROPERTY("startColor", &ParticleComponent::startColor)
    PROPERTY("startColorAlpha", &ParticleComponent::startColorAlpha)
    PROPERTY("endColor", &ParticleComponent::endColor)
    PROPERTY("endColorAlpha", &ParticleComponent::endColorAlpha)
    // Physics
    PROPERTY("gravity", &ParticleComponent::gravity)
    PROPERTY("velocityRandomness", &ParticleComponent::velocityRandomness)
    PROPERTY("initialVelocity", &ParticleComponent::initialVelocity)
    PROPERTY("timeSinceEmission", &ParticleComponent::timeSinceEmission)
    // Runtime control
    PROPERTY("isEmitting", &ParticleComponent::isEmitting)
    PROPERTY("texturePath", &ParticleComponent::texturePath)
    PROPERTY("isPlayingInEditor", &ParticleComponent::isPlayingInEditor)
    PROPERTY("isPausedInEditor", &ParticleComponent::isPausedInEditor)
END_COMPONENT()

BEGIN_COMPONENT(SpriteRenderComponent, "SpriteRenderComponent")
    // Transform
    PROPERTY("position", &SpriteRenderComponent::position)
    PROPERTY("scale", &SpriteRenderComponent::scale)
    PROPERTY("rotation", &SpriteRenderComponent::rotation)
    // Appearance
    PROPERTY("color", &SpriteRenderComponent::color)
    PROPERTY("alpha", &SpriteRenderComponent::alpha)
    // Rendering mode
    PROPERTY("is3D", &SpriteRenderComponent::is3D)
    PROPERTY("enableBillboard", &SpriteRenderComponent::enableBillboard)
    // Sorting
    PROPERTY("sortingLayer", &SpriteRenderComponent::sortingLayer)
    PROPERTY("sortingOrder", &SpriteRenderComponent::sortingOrder)
    PROPERTY("saved3DPosition", &SpriteRenderComponent::saved3DPosition)
    // Asset path (read-only for display)
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

BEGIN_COMPONENT(ModelRenderComponent, "ModelRenderComponent")
    PROPERTY("isVisible", &ModelRenderComponent::isVisible)
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
    METHOD("SetEnabled", &ColliderComponent::SetEnabled)
    METHOD("IsEnabled", &ColliderComponent::IsEnabled)
    PROPERTY("center", &ColliderComponent::center)
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
    METHOD("SetEnabled", &RigidBodyComponent::SetEnabled)
    METHOD("IsEnabled", &RigidBodyComponent::IsEnabled)
    METHOD("AddForce", &RigidBodyComponent::AddForce)
    METHOD("AddTorque", &RigidBodyComponent::AddTorque)
    METHOD("AddImpulse", &RigidBodyComponent::AddImpulse)
END_COMPONENT()

// ============================================================================
// Audio Components
// ============================================================================

BEGIN_COMPONENT(AudioComponent, "AudioComponent")
    // Enable/disable
    PROPERTY("enabled", &AudioComponent::enabled)
    // Audio settings
    PROPERTY("Mute", &AudioComponent::Mute)
    PROPERTY("bypassListenerEffects", &AudioComponent::bypassListenerEffects)
    PROPERTY("PlayOnAwake", &AudioComponent::PlayOnAwake)
    PROPERTY("Loop", &AudioComponent::Loop)
    PROPERTY("Priority", &AudioComponent::Priority)
    PROPERTY("Volume", &AudioComponent::Volume)
    PROPERTY("Pitch", &AudioComponent::Pitch)
    PROPERTY("StereoPan", &AudioComponent::StereoPan)
    PROPERTY("reverbZoneMix", &AudioComponent::reverbZoneMix)
    PROPERTY("OutputAudioMixerGroup", &AudioComponent::OutputAudioMixerGroup)
    // 3D Audio settings
    PROPERTY("Spatialize", &AudioComponent::Spatialize)
    PROPERTY("SpatialBlend", &AudioComponent::SpatialBlend)
    PROPERTY("DopplerLevel", &AudioComponent::DopplerLevel)
    PROPERTY("MinDistance", &AudioComponent::MinDistance)
    PROPERTY("MaxDistance", &AudioComponent::MaxDistance)
    // State (read-only properties via methods)
    PROPERTY("IsPlaying", &AudioComponent::IsPlaying)
    PROPERTY("IsPaused", &AudioComponent::IsPaused)
    PROPERTY("Position", &AudioComponent::Position)
    // Playback methods
    METHOD("Play", &AudioComponent::Play)
    METHOD("PlayOneShot", &AudioComponent::PlayOneShot)
    METHOD("Stop", &AudioComponent::Stop)
    METHOD("Pause", &AudioComponent::Pause)
    METHOD("UnPause", &AudioComponent::UnPause)
    METHOD("GetIsPlaying", &AudioComponent::GetIsPlaying)
    METHOD("GetIsPaused", &AudioComponent::GetIsPaused)
    // Settings methods
    METHOD("SetVolume", &AudioComponent::SetVolume)
    METHOD("SetPitch", &AudioComponent::SetPitch)
    METHOD("SetLoop", &AudioComponent::SetLoop)
    METHOD("SetMute", &AudioComponent::SetMute)
    METHOD("SetClip", &AudioComponent::SetClipFromString)
END_COMPONENT()

BEGIN_COMPONENT(AudioListenerComponent, "AudioListenerComponent")
    PROPERTY("enabled", &AudioListenerComponent::enabled)
END_COMPONENT()

BEGIN_COMPONENT(AudioReverbZoneComponent, "AudioReverbZoneComponent")
    PROPERTY("enabled", &AudioReverbZoneComponent::enabled)
    PROPERTY("MinDistance", &AudioReverbZoneComponent::MinDistance)
    PROPERTY("MaxDistance", &AudioReverbZoneComponent::MaxDistance)
    PROPERTY("reverbPresetIndex", &AudioReverbZoneComponent::reverbPresetIndex)
    // Detailed reverb parameters
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
// UI Components
// ============================================================================

BEGIN_COMPONENT(SliderComponent, "SliderComponent")
    PROPERTY("minValue", &SliderComponent::minValue)
    PROPERTY("maxValue", &SliderComponent::maxValue)
    PROPERTY("value", &SliderComponent::value)
    PROPERTY("wholeNumbers", &SliderComponent::wholeNumbers)
    PROPERTY("interactable", &SliderComponent::interactable)
    PROPERTY("horizontal", &SliderComponent::horizontal)
END_COMPONENT()

// ============================================================================
// Tag and Layer Components
// ============================================================================

BEGIN_COMPONENT(TagComponent, "TagComponent")
    PROPERTY("tagIndex", &TagComponent::tagIndex)
    // Methods
    METHOD("SetTag", &TagComponent::SetTag)
    METHOD("GetTagName", &TagComponent::GetTagName)
    METHOD("HasTag", &TagComponent::HasTag)
END_COMPONENT()

BEGIN_COMPONENT(LayerComponent, "LayerComponent")
    PROPERTY("layerIndex", &LayerComponent::layerIndex)
    // Methods
    METHOD("SetLayer", &LayerComponent::SetLayer)
    METHOD("GetLayerName", &LayerComponent::GetLayerName)
    METHOD("IsInLayer", &LayerComponent::IsInLayer)
    METHOD("GetLayerMask", &LayerComponent::GetLayerMask)
END_COMPONENT()

BEGIN_COMPONENT(ButtonComponent, "ButtonComponent")
    PROPERTY("interactable", &ButtonComponent::interactable)
END_COMPONENT()

BEGIN_COMPONENT(ActiveComponent, "ActiveComponent")
    PROPERTY("isActive", &ActiveComponent::isActive)
END_COMPONENT()





// ============================================================================
// Hierarchy Components (read-only for Lua - managed by engine)
// ============================================================================

BEGIN_COMPONENT(SiblingIndexComponent, "SiblingIndexComponent")
    PROPERTY("siblingIndex", &SiblingIndexComponent::siblingIndex)
END_COMPONENT()

// Add more BEGIN_COMPONENT(...) ... PROPERTY(...) ... METHOD(...) ... END_COMPONENT() blocks here.
