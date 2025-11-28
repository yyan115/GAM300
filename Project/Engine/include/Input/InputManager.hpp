#pragma once

#include <unordered_map>
#include <WindowManager.hpp>
#include "RunTimeVar.hpp"
#include "Keys.h"

class IPlatform; // Forward declaration

class InputManager {
public:
	static void Initialize();  // No longer needs window parameter
	static void Update();

	static bool GetKeyDown(Input::Key key);
	static bool GetKey(Input::Key key);

	static bool GetMouseButtonDown(Input::MouseButton button);
	static bool GetMouseButton(Input::MouseButton button);

	static double GetMouseX();
	static double GetMouseY();

	static bool GetAnyKeyDown();
	static bool GetAnyMouseButtonDown();
	static bool GetAnyInputDown();

	// Platform callback methods - called by platform implementations
	static void OnKeyEvent(Input::Key key, Input::KeyAction action);
	static void OnMouseButtonEvent(Input::MouseButton button, Input::KeyAction action);
	static void OnMousePositionEvent(double x, double y);
	static void OnScrollEvent(double xOffset, double yOffset);

	ENGINE_API static void SetGamePanelMousePos(float newX, float newY);

private:
	static IPlatform* platform;  // Reference to platform for input polling
	
	//// Internal state tracking using engine key constants
	static std::unordered_map<Input::Key, bool> keyStates;
	static std::unordered_map<Input::MouseButton, bool> mouseButtonStates;
	
	static std::unordered_map<Input::Key, bool> prevKeyStates;
	static std::unordered_map<Input::MouseButton, bool> prevMouseButtonStates;

	static double gamePanelMouseX, gamePanelMouseY;

	//static double mouseX;
	//static double mouseY;
	//static double scrollOffsetX;
	//static double scrollOffsetY;
};