#include "pch.h"
#include "Physics/RigidBodyComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(RigidBodyComponent)
	REFL_REGISTER_PROPERTY(enabled)
	REFL_REGISTER_PROPERTY(motionID)
	REFL_REGISTER_PROPERTY(ccd)
	REFL_REGISTER_PROPERTY(gravityFactor)
	REFL_REGISTER_PROPERTY(angularVel)
	REFL_REGISTER_PROPERTY(linearVel)
	REFL_REGISTER_PROPERTY(isTrigger)
	REFL_REGISTER_PROPERTY(linearDamping)
	REFL_REGISTER_PROPERTY(angularDamping)

REFL_REGISTER_END;
#pragma endregion