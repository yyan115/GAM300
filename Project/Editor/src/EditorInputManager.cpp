#include "EditorInputManager.hpp"
#include <algorithm>

// Static member definitions
EditorInputManager::MouseState EditorInputManager::mouseState;
EditorInputManager::KeyboardState EditorInputManager::keyboardState;

void EditorInputManager::Update() {
    UpdateMouseState();
    UpdateKeyboardState();
}

void EditorInputManager::UpdateMouseState() {
    ImGuiIO& io = ImGui::GetIO();

    // Get current mouse position
    glm::vec2 currentPos(io.MousePos.x, io.MousePos.y);

    // Calculate mouse delta (same as the original working method)
    if (!mouseState.firstMouse) {
        mouseState.delta = currentPos - mouseState.lastPosition;
    } else {
        mouseState.delta = glm::vec2(0.0f);
        mouseState.firstMouse = false;
    }

    // Update positions
    mouseState.position = currentPos;
    mouseState.lastPosition = currentPos;

    // Update scroll
    mouseState.scrollDelta = io.MouseWheel;

    // Update mouse button states (pressed = just pressed this frame)
    bool currentLeft = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool currentMiddle = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    bool currentRight = ImGui::IsMouseDown(ImGuiMouseButton_Right);

    mouseState.leftPressed = currentLeft && !mouseState.leftDown;
    mouseState.middlePressed = currentMiddle && !mouseState.middleDown;
    mouseState.rightPressed = currentRight && !mouseState.rightDown;

    mouseState.leftDown = currentLeft;
    mouseState.middleDown = currentMiddle;
    mouseState.rightDown = currentRight;
}

void EditorInputManager::UpdateKeyboardState() {
    ImGuiIO& io = ImGui::GetIO();

    // Update modifier keys
    keyboardState.altPressed = io.KeyAlt;
    keyboardState.ctrlPressed = io.KeyCtrl;
    keyboardState.shiftPressed = io.KeyShift;

    // Update editor shortcut keys (only register as pressed if just pressed this frame)
    keyboardState.qKeyPressed = ImGui::IsKeyPressed(ImGuiKey_Q);  // Normal/panning mode
    keyboardState.wKeyPressed = ImGui::IsKeyPressed(ImGuiKey_W);  // Translate
    keyboardState.eKeyPressed = ImGui::IsKeyPressed(ImGuiKey_E);  // Rotate
    keyboardState.rKeyPressed = ImGui::IsKeyPressed(ImGuiKey_R);  // Scale
}

const EditorInputManager::MouseState& EditorInputManager::GetMouseState() {
    return mouseState;
}

const EditorInputManager::KeyboardState& EditorInputManager::GetKeyboardState() {
    return keyboardState;
}

bool EditorInputManager::IsWindowHovered(const char* windowName) {
    // Check if the specific window is hovered
    // For now, we'll use ImGui::IsWindowHovered() but this could be enhanced
    // to check specific window names if needed
    return ImGui::IsWindowHovered();
}

glm::vec2 EditorInputManager::GetMouseDelta(float sensitivity) {
    return mouseState.delta * sensitivity;
}

bool EditorInputManager::ShouldHandleCameraInput(const char* windowName) {
    // Only handle camera input if the specified window is hovered
    // Allow camera input unless we're typing in a text field
    return ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered();
}

bool EditorInputManager::IsGizmoShortcutPressed(int gizmoType) {
    switch (gizmoType) {
        case 0: return keyboardState.qKeyPressed;  // Normal/panning mode
        case 1: return keyboardState.wKeyPressed;  // Translate
        case 2: return keyboardState.eKeyPressed;  // Rotate
        case 3: return keyboardState.rKeyPressed;  // Scale
        default: return false;
    }
}

