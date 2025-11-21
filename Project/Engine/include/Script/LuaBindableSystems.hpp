#pragma once

// ============================================================================
// Input System
// ============================================================================
#include "Input/InputManager.hpp"

#pragma once
#include "Input/Keys.h"
#include "Input/InputManager.hpp"

// Create integer constants for each enum value that LuaBridge can bind to
namespace InputEnumStorage {
    // Keys - cast to int for LuaBridge
    static constexpr int KEY_A = static_cast<int>(Input::Key::A);
    static constexpr int KEY_B = static_cast<int>(Input::Key::B);
    static constexpr int KEY_C = static_cast<int>(Input::Key::C);
    static constexpr int KEY_D = static_cast<int>(Input::Key::D);
    static constexpr int KEY_E = static_cast<int>(Input::Key::E);
    static constexpr int KEY_F = static_cast<int>(Input::Key::F);
    static constexpr int KEY_G = static_cast<int>(Input::Key::G);
    static constexpr int KEY_H = static_cast<int>(Input::Key::H);
    static constexpr int KEY_I = static_cast<int>(Input::Key::I);
    static constexpr int KEY_J = static_cast<int>(Input::Key::J);
    static constexpr int KEY_K = static_cast<int>(Input::Key::K);
    static constexpr int KEY_L = static_cast<int>(Input::Key::L);
    static constexpr int KEY_M = static_cast<int>(Input::Key::M);
    static constexpr int KEY_N = static_cast<int>(Input::Key::N);
    static constexpr int KEY_O = static_cast<int>(Input::Key::O);
    static constexpr int KEY_P = static_cast<int>(Input::Key::P);
    static constexpr int KEY_Q = static_cast<int>(Input::Key::Q);
    static constexpr int KEY_R = static_cast<int>(Input::Key::R);
    static constexpr int KEY_S = static_cast<int>(Input::Key::S);
    static constexpr int KEY_T = static_cast<int>(Input::Key::T);
    static constexpr int KEY_U = static_cast<int>(Input::Key::U);
    static constexpr int KEY_V = static_cast<int>(Input::Key::V);
    static constexpr int KEY_W = static_cast<int>(Input::Key::W);
    static constexpr int KEY_X = static_cast<int>(Input::Key::X);
    static constexpr int KEY_Y = static_cast<int>(Input::Key::Y);
    static constexpr int KEY_Z = static_cast<int>(Input::Key::Z);

    static constexpr int KEY_NUM_0 = static_cast<int>(Input::Key::NUM_0);
    static constexpr int KEY_NUM_1 = static_cast<int>(Input::Key::NUM_1);
    static constexpr int KEY_NUM_2 = static_cast<int>(Input::Key::NUM_2);
    static constexpr int KEY_NUM_3 = static_cast<int>(Input::Key::NUM_3);
    static constexpr int KEY_NUM_4 = static_cast<int>(Input::Key::NUM_4);
    static constexpr int KEY_NUM_5 = static_cast<int>(Input::Key::NUM_5);
    static constexpr int KEY_NUM_6 = static_cast<int>(Input::Key::NUM_6);
    static constexpr int KEY_NUM_7 = static_cast<int>(Input::Key::NUM_7);
    static constexpr int KEY_NUM_8 = static_cast<int>(Input::Key::NUM_8);
    static constexpr int KEY_NUM_9 = static_cast<int>(Input::Key::NUM_9);

    static constexpr int KEY_SPACE = static_cast<int>(Input::Key::SPACE);
    static constexpr int KEY_ENTER = static_cast<int>(Input::Key::ENTER);
    //static constexpr int KEY_ESCAPE = static_cast<int>(Input::Key::ESCAPE);
    static constexpr int KEY_TAB = static_cast<int>(Input::Key::TAB);
    static constexpr int KEY_BACKSPACE = static_cast<int>(Input::Key::BACKSPACE);
    //static constexpr int KEY_DELETE = static_cast<int>(Input::Key::DELETE_KEY);

    //static constexpr int KEY_LEFT_SHIFT = static_cast<int>(Input::Key::LEFT_SHIFT);
    //static constexpr int KEY_RIGHT_SHIFT = static_cast<int>(Input::Key::RIGHT_SHIFT);
    //static constexpr int KEY_LEFT_CONTROL = static_cast<int>(Input::Key::LEFT_CONTROL);
    //static constexpr int KEY_RIGHT_CONTROL = static_cast<int>(Input::Key::RIGHT_CONTROL);
    //static constexpr int KEY_LEFT_ALT = static_cast<int>(Input::Key::LEFT_ALT);
    //static constexpr int KEY_RIGHT_ALT = static_cast<int>(Input::Key::RIGHT_ALT);

    static constexpr int KEY_LEFT = static_cast<int>(Input::Key::LEFT);
    static constexpr int KEY_RIGHT = static_cast<int>(Input::Key::RIGHT);
    static constexpr int KEY_UP = static_cast<int>(Input::Key::UP);
    static constexpr int KEY_DOWN = static_cast<int>(Input::Key::DOWN);

    static constexpr int KEY_F1 = static_cast<int>(Input::Key::F1);
    static constexpr int KEY_F2 = static_cast<int>(Input::Key::F2);
    static constexpr int KEY_F3 = static_cast<int>(Input::Key::F3);
    static constexpr int KEY_F4 = static_cast<int>(Input::Key::F4);
    static constexpr int KEY_F5 = static_cast<int>(Input::Key::F5);
    static constexpr int KEY_F6 = static_cast<int>(Input::Key::F6);
    static constexpr int KEY_F7 = static_cast<int>(Input::Key::F7);
    static constexpr int KEY_F8 = static_cast<int>(Input::Key::F8);
    static constexpr int KEY_F9 = static_cast<int>(Input::Key::F9);
    static constexpr int KEY_F10 = static_cast<int>(Input::Key::F10);
    static constexpr int KEY_F11 = static_cast<int>(Input::Key::F11);
    static constexpr int KEY_F12 = static_cast<int>(Input::Key::F12);

    // Mouse buttons
    static constexpr int MOUSE_LEFT = static_cast<int>(Input::MouseButton::LEFT);
    static constexpr int MOUSE_RIGHT = static_cast<int>(Input::MouseButton::RIGHT);
    static constexpr int MOUSE_MIDDLE = static_cast<int>(Input::MouseButton::MIDDLE);
}