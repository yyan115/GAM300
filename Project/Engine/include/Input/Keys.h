#pragma once

// Engine-specific input constants that work across all platforms
namespace Input {

enum class Key {
    // Alphabet keys
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    
    // Number keys
    NUM_0, NUM_1, NUM_2, NUM_3, NUM_4, NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,
    
    // Special keys
    SPACE,
    ENTER,
    ESC,
    TAB,
    BACKSPACE,
    DELETE_,
    
    // Arrow keys
    UP, DOWN, LEFT, RIGHT,
    
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    
    // Modifier keys
    SHIFT, CTRL, ALT,
    
    UNKNOWN
};

enum class MouseButton {
    LEFT,
    RIGHT, 
    MIDDLE,
    BUTTON_4,
    BUTTON_5,
    UNKNOWN
};

enum class KeyAction {
    PRESS,
    RELEASE,
    REPEAT
};

} // namespace Input