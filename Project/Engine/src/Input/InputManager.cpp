#include "pch.h"

#include <Input/InputManager.hpp>
#include "Platform/IPlatform.h"
#include "WindowManager.hpp"

// Static member definitions using our new engine key constants
IPlatform* InputManager::platform = nullptr;
std::unordered_map<Input::Key, bool> InputManager::keyStates;
std::unordered_map<Input::MouseButton, bool> InputManager::mouseButtonStates;
std::unordered_map<Input::Key, bool> InputManager::prevKeyStates;
std::unordered_map<Input::MouseButton, bool> InputManager::prevMouseButtonStates;
double InputManager::mouseX = 0.0;
double InputManager::mouseY = 0.0;
double InputManager::scrollOffsetX = 0.0;
double InputManager::scrollOffsetY = 0.0;

void InputManager::Initialize()
{
	// Get platform reference from WindowManager (will implement WindowManager::GetPlatform() later)
	// For now, this is a placeholder - we'll need to implement this properly
	platform = nullptr; // TODO: WindowManager::GetPlatform();
}

void InputManager::Update()
{
	// Store previous frame state for GetKeyDown/GetMouseButtonDown detection
	prevKeyStates = keyStates;
	prevMouseButtonStates = mouseButtonStates;
	
	// Input is updated through callbacks (OnKeyEvent, OnMouseButtonEvent, etc.)
	// No polling needed - events are pushed to us by the platform
}

bool InputManager::GetKeyDown(Input::Key key)
{
	return keyStates[key] && !prevKeyStates[key];
}

bool InputManager::GetKey(Input::Key key)
{
	auto it = keyStates.find(key);
	return it != keyStates.end() && it->second;
}

bool InputManager::GetMouseButtonDown(Input::MouseButton button)
{
	return mouseButtonStates[button] && !prevMouseButtonStates[button];
}

bool InputManager::GetMouseButton(Input::MouseButton button)
{
	auto it = mouseButtonStates.find(button);
	return it != mouseButtonStates.end() && it->second;
}

double InputManager::GetMouseX()
{
	return mouseX;
}

double InputManager::GetMouseY()
{
	return mouseY;
}

bool InputManager::GetAnyKeyDown()
{
	for (const auto& keyState : keyStates) {
		if (GetKeyDown(keyState.first)) {
			return true;
		}
	}

	return false;
}

bool InputManager::GetAnyMouseButtonDown()
{
	for (const auto& mouseState : mouseButtonStates) {
		if (GetMouseButtonDown(mouseState.first)) {
			return true;
		}
	}

	return false;
}

bool InputManager::GetAnyInputDown()
{
	return GetAnyKeyDown() || GetAnyMouseButtonDown();
}

// Platform callback implementations - called by platform when events occur
void InputManager::OnKeyEvent(Input::Key key, Input::KeyAction action)
{
	keyStates[key] = (action != Input::KeyAction::RELEASE);
}

void InputManager::OnMouseButtonEvent(Input::MouseButton button, Input::KeyAction action)
{
	mouseButtonStates[button] = (action != Input::KeyAction::RELEASE);
}

void InputManager::OnMousePositionEvent(double x, double y)
{
	mouseX = x;
	mouseY = y;
}

void InputManager::OnScrollEvent(double xOffset, double yOffset)
{
	scrollOffsetX += xOffset;
	scrollOffsetY += yOffset;
}
