#include "pch.h"
#include "Graphics/DebugDraw/DebugDrawComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(DebugDrawData)
	//REFL_REGISTER_PROPERTY(type)
	//REFL_REGISTER_PROPERTY(position)
	//REFL_REGISTER_PROPERTY(scale)
	//REFL_REGISTER_PROPERTY(rotation)
	//REFL_REGISTER_PROPERTY(color)
	REFL_REGISTER_PROPERTY(duration)
	REFL_REGISTER_PROPERTY(lineWidth)
	//REFL_REGISTER_PROPERTY(endPosition)
	//REFL_REGISTER_PROPERTY(meshModel)
REFL_REGISTER_END;

REFL_REGISTER_START(DebugDrawComponent)
	//REFL_REGISTER_PROPERTY(drawCommands) //Causing issues - Reflection Cannot add properties of child classes that arent fully serialized
REFL_REGISTER_END;
#pragma endregion

