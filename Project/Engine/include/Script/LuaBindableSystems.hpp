#pragma once

#include "Input/Keys.h"
#include "Input/InputManager.hpp"

// Wrapper functions to convert int to enum for Input functions
namespace InputWrappers {
    inline bool GetKey(int key) {
        return InputManager::GetKey(static_cast<Input::Key>(key));
    }

    inline bool GetKeyDown(int key) {
        return InputManager::GetKeyDown(static_cast<Input::Key>(key));
    }

    inline bool GetMouseButton(int button) {
        return InputManager::GetMouseButton(static_cast<Input::MouseButton>(button));
    }

    inline bool GetMouseButtonDown(int button) {
        return InputManager::GetMouseButtonDown(static_cast<Input::MouseButton>(button));
    }
}