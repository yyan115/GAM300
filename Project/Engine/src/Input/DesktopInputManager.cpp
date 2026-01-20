#include "pch.h"
#include "Input/DesktopInputManager.h"
#include "Platform/IPlatform.h"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <iostream>

// ========== Constructor ==========

DesktopInputManager::DesktopInputManager(IPlatform* platform)
    : m_platform(platform)
{
    if (!m_platform) {
        std::cerr << "[DesktopInputManager] ERROR: Platform pointer is null!" << std::endl;
    }
}

// ========== IInputSystem Interface Implementation ==========

bool DesktopInputManager::IsActionPressed(const std::string& action) {
    return m_currentActions.count(action) > 0;
}

bool DesktopInputManager::IsActionJustPressed(const std::string& action) {
    return m_currentActions.count(action) > 0 &&
           m_previousActions.count(action) == 0;
}

bool DesktopInputManager::IsActionJustReleased(const std::string& action) {
    return m_currentActions.count(action) == 0 &&
           m_previousActions.count(action) > 0;
}

glm::vec2 DesktopInputManager::GetAxis(const std::string& axisName) {
    auto it = m_axisBindings.find(axisName);
    if (it == m_axisBindings.end()) {
        return glm::vec2(0.0f);
    }

    const AxisBinding& binding = it->second;

    switch (binding.type) {
        case AxisType::KeyboardComposite:
            return EvaluateKeyboardAxis(binding);

        case AxisType::MouseDelta:
            return m_mouseDelta * binding.sensitivity;

        case AxisType::Gamepad:
            // TODO: Implement gamepad support
            return glm::vec2(0.0f);

        default:
            return glm::vec2(0.0f);
    }
}

bool DesktopInputManager::IsPointerPressed() {
    return m_pointerPressed;
}

bool DesktopInputManager::IsPointerJustPressed() {
    return m_pointerPressed && !m_pointerPreviouslyPressed;
}

glm::vec2 DesktopInputManager::GetPointerPosition() {
    return GetMousePositionNormalized();
}

int DesktopInputManager::GetTouchCount() {
    // Desktop: Treat mouse as single touch point
    return m_pointerPressed ? 1 : 0;
}

glm::vec2 DesktopInputManager::GetTouchPosition(int index) {
    if (index == 0 && m_pointerPressed) {
        return GetMousePositionNormalized();
    }
    return glm::vec2(0.0f);
}

void DesktopInputManager::Update(float deltaTime) {
    if (!m_platform) return;

    UpdateActionStates();
    UpdateAxisStates(deltaTime);

    // Update pointer state
    m_pointerPreviouslyPressed = m_pointerPressed;
    m_pointerPressed = IsMouseButtonPressed(Input::MouseButton::LEFT);
}

bool DesktopInputManager::LoadConfig(const std::string& path) {
    std::cout << "[DesktopInputManager] Loading config from: " << path << std::endl;

    if (!m_platform) {
        std::cerr << "[DesktopInputManager] ERROR: Platform is null!" << std::endl;
        return false;
    }

    // Check if file exists
    if (!m_platform->FileExists(path)) {
        std::cerr << "[DesktopInputManager] ERROR: Config file not found: " << path << std::endl;
        return false;
    }

    // Load file using platform
    std::vector<uint8_t> configData = m_platform->ReadAsset(path);
    if (configData.empty()) {
        std::cerr << "[DesktopInputManager] ERROR: Failed to read config file: " << path << std::endl;
        return false;
    }

    // Parse JSON
    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(configData.data()), configData.size());

    if (doc.HasParseError()) {
        std::cerr << "[DesktopInputManager] ERROR: JSON parse error at offset "
                  << doc.GetErrorOffset() << ": "
                  << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return false;
    }

    // ===== Load Actions =====
    // New format: actions at root level, with "desktop" nested inside each action
    if (doc.HasMember("actions") && doc["actions"].IsObject()) {
        const auto& actions = doc["actions"];

        for (auto it = actions.MemberBegin(); it != actions.MemberEnd(); ++it) {
            std::string actionName = it->name.GetString();
            const auto& actionData = it->value;

            // Skip if no desktop binding
            if (!actionData.HasMember("desktop") || !actionData["desktop"].IsObject()) {
                std::cout << "[DesktopInputManager] Skipping action '" << actionName << "' (no desktop binding)" << std::endl;
                continue;
            }

            const auto& desktopBinding = actionData["desktop"];
            ActionBinding binding;

            // Parse keys
            if (desktopBinding.HasMember("keys") && desktopBinding["keys"].IsArray()) {
                const auto& keys = desktopBinding["keys"];
                for (rapidjson::SizeType i = 0; i < keys.Size(); ++i) {
                    if (keys[i].IsString()) {
                        std::string keyName = keys[i].GetString();
                        Input::Key key = ParseKey(keyName);
                        if (key != Input::Key::UNKNOWN) {
                            binding.keys.push_back(key);
                        } else {
                            std::cerr << "[DesktopInputManager] WARNING: Unknown key: " << keyName << std::endl;
                        }
                    }
                }
            }

            // Parse mouse buttons
            if (desktopBinding.HasMember("mouseButtons") && desktopBinding["mouseButtons"].IsArray()) {
                const auto& buttons = desktopBinding["mouseButtons"];
                for (rapidjson::SizeType i = 0; i < buttons.Size(); ++i) {
                    if (buttons[i].IsString()) {
                        std::string buttonName = buttons[i].GetString();
                        Input::MouseButton button = ParseMouseButton(buttonName);
                        if (button != Input::MouseButton::UNKNOWN) {
                            binding.mouseButtons.push_back(button);
                        } else {
                            std::cerr << "[DesktopInputManager] WARNING: Unknown mouse button: " << buttonName << std::endl;
                        }
                    }
                }
            }

            m_actionBindings[actionName] = binding;
            std::cout << "[DesktopInputManager] Loaded action: " << actionName
                      << " (" << binding.keys.size() << " keys, "
                      << binding.mouseButtons.size() << " buttons)" << std::endl;
        }
    }

    // ===== Load Axes =====
    // New format: axes at root level, with "desktop" nested inside each axis
    if (doc.HasMember("axes") && doc["axes"].IsObject()) {
        const auto& axes = doc["axes"];

        for (auto it = axes.MemberBegin(); it != axes.MemberEnd(); ++it) {
            std::string axisName = it->name.GetString();
            const auto& axisData = it->value;

            // Skip if no desktop binding
            if (!axisData.HasMember("desktop") || !axisData["desktop"].IsObject()) {
                std::cout << "[DesktopInputManager] Skipping axis '" << axisName << "' (no desktop binding)" << std::endl;
                continue;
            }

            const auto& desktopBinding = axisData["desktop"];
            AxisBinding binding;

            // Check axis type
            if (desktopBinding.HasMember("type") && desktopBinding["type"].IsString()) {
                std::string type = desktopBinding["type"].GetString();

                if (type == "mouse_delta") {
                    binding.type = AxisType::MouseDelta;

                    if (desktopBinding.HasMember("sensitivity") && desktopBinding["sensitivity"].IsNumber()) {
                        binding.sensitivity = desktopBinding["sensitivity"].GetFloat();
                    }

                    std::cout << "[DesktopInputManager] Loaded axis: " << axisName
                              << " (mouse_delta, sensitivity=" << binding.sensitivity << ")" << std::endl;
                }
                else if (type == "keyboard") {
                    binding.type = AxisType::KeyboardComposite;

                    // Parse directional keys (new format: up/down/left/right)
                    auto parseKeyArray = [this](const rapidjson::Value& arr, std::vector<Input::Key>& outKeys) {
                        if (arr.IsArray()) {
                            for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
                                if (arr[i].IsString()) {
                                    Input::Key key = ParseKey(arr[i].GetString());
                                    if (key != Input::Key::UNKNOWN) {
                                        outKeys.push_back(key);
                                    }
                                }
                            }
                        }
                    };

                    // New naming: up/down/left/right (maps to positiveY/negativeY/negativeX/positiveX)
                    if (desktopBinding.HasMember("right")) parseKeyArray(desktopBinding["right"], binding.positiveX);
                    if (desktopBinding.HasMember("left")) parseKeyArray(desktopBinding["left"], binding.negativeX);
                    if (desktopBinding.HasMember("up")) parseKeyArray(desktopBinding["up"], binding.positiveY);
                    if (desktopBinding.HasMember("down")) parseKeyArray(desktopBinding["down"], binding.negativeY);

                    std::cout << "[DesktopInputManager] Loaded axis: " << axisName << " (keyboard)" << std::endl;
                }
                else if (type == "gamepad") {
                    binding.type = AxisType::Gamepad;
                    std::cout << "[DesktopInputManager] Loaded axis: " << axisName
                              << " (gamepad - not yet implemented)" << std::endl;
                }
            }

            m_axisBindings[axisName] = binding;
        }
    }

    std::cout << "[DesktopInputManager] Config loaded successfully. "
              << m_actionBindings.size() << " actions, "
              << m_axisBindings.size() << " axes" << std::endl;

    return true;
}

std::unordered_map<std::string, bool> DesktopInputManager::GetAllActionStates() {
    std::unordered_map<std::string, bool> states;

    // Include all configured actions (false if not pressed)
    for (const auto& [actionName, binding] : m_actionBindings) {
        states[actionName] = IsActionPressed(actionName);
    }

    return states;
}

std::unordered_map<std::string, glm::vec2> DesktopInputManager::GetAllAxisStates() {
    std::unordered_map<std::string, glm::vec2> states;

    for (const auto& [axisName, binding] : m_axisBindings) {
        states[axisName] = GetAxis(axisName);
    }

    return states;
}

void DesktopInputManager::RenderOverlay(int screenWidth, int screenHeight) {
    // Desktop has no virtual controls to render
}

// ========== Private Helper Methods ==========

void DesktopInputManager::UpdateActionStates() {
    // Save previous state
    m_previousActions = m_currentActions;
    m_currentActions.clear();

    // Evaluate all action bindings
    for (const auto& [actionName, binding] : m_actionBindings) {
        bool isPressed = false;

        // Check if any bound key is pressed
        for (Input::Key key : binding.keys) {
            if (IsKeyPressed(key)) {
                isPressed = true;
                break;
            }
        }

        // Check if any bound mouse button is pressed
        if (!isPressed) {
            for (Input::MouseButton button : binding.mouseButtons) {
                if (IsMouseButtonPressed(button)) {
                    isPressed = true;
                    break;
                }
            }
        }

        if (isPressed) {
            m_currentActions.insert(actionName);
        }
    }
}

void DesktopInputManager::UpdateAxisStates(float deltaTime) {
    // Update mouse delta for MouseDelta axes
    glm::vec2 currentMousePos = GetMousePositionNormalized();

    if (m_firstMouseUpdate) {
        m_previousMousePos = currentMousePos;
        m_mouseDelta = glm::vec2(0.0f);
        m_firstMouseUpdate = false;
    } else {
        m_mouseDelta = currentMousePos - m_previousMousePos;
        m_previousMousePos = currentMousePos;
    }
}

glm::vec2 DesktopInputManager::EvaluateKeyboardAxis(const AxisBinding& binding) {
    glm::vec2 axis(0.0f);

    // X axis
    for (Input::Key key : binding.positiveX) {
        if (IsKeyPressed(key)) {
            axis.x += 1.0f;
            break;
        }
    }
    for (Input::Key key : binding.negativeX) {
        if (IsKeyPressed(key)) {
            axis.x -= 1.0f;
            break;
        }
    }

    // Y axis
    for (Input::Key key : binding.positiveY) {
        if (IsKeyPressed(key)) {
            axis.y += 1.0f;
            break;
        }
    }
    for (Input::Key key : binding.negativeY) {
        if (IsKeyPressed(key)) {
            axis.y -= 1.0f;
            break;
        }
    }

    // Normalize diagonal movement (WASD gives √2, should be 1)
    float length = glm::length(axis);
    if (length > 1.0f) {
        axis /= length;
    }

    return axis;
}

bool DesktopInputManager::IsKeyPressed(Input::Key key) {
    if (!m_platform) return false;
    return m_platform->IsKeyPressed(key);
}

bool DesktopInputManager::IsMouseButtonPressed(Input::MouseButton button) {
    if (!m_platform) return false;
    return m_platform->IsMouseButtonPressed(button);
}

glm::vec2 DesktopInputManager::GetMousePositionNormalized() {
    if (!m_platform) return glm::vec2(0.0f);

    double mouseX, mouseY;

#ifdef EDITOR
    // In editor, use game panel mouse position when cursor is not locked
    if (!WindowManager::IsCursorLocked()) {
        mouseX = m_gamePanelMouseX;
        mouseY = m_gamePanelMouseY;
    } else {
        m_platform->GetMousePosition(&mouseX, &mouseY);
    }
#else
    // In runtime, always use raw platform mouse position
    m_platform->GetMousePosition(&mouseX, &mouseY);
#endif

    return glm::vec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
}

Input::Key DesktopInputManager::ParseKey(const std::string& keyName) {
    // Alphabet
    if (keyName == "A") return Input::Key::A;
    if (keyName == "B") return Input::Key::B;
    if (keyName == "C") return Input::Key::C;
    if (keyName == "D") return Input::Key::D;
    if (keyName == "E") return Input::Key::E;
    if (keyName == "F") return Input::Key::F;
    if (keyName == "G") return Input::Key::G;
    if (keyName == "H") return Input::Key::H;
    if (keyName == "I") return Input::Key::I;
    if (keyName == "J") return Input::Key::J;
    if (keyName == "K") return Input::Key::K;
    if (keyName == "L") return Input::Key::L;
    if (keyName == "M") return Input::Key::M;
    if (keyName == "N") return Input::Key::N;
    if (keyName == "O") return Input::Key::O;
    if (keyName == "P") return Input::Key::P;
    if (keyName == "Q") return Input::Key::Q;
    if (keyName == "R") return Input::Key::R;
    if (keyName == "S") return Input::Key::S;
    if (keyName == "T") return Input::Key::T;
    if (keyName == "U") return Input::Key::U;
    if (keyName == "V") return Input::Key::V;
    if (keyName == "W") return Input::Key::W;
    if (keyName == "X") return Input::Key::X;
    if (keyName == "Y") return Input::Key::Y;
    if (keyName == "Z") return Input::Key::Z;

    // Numbers
    if (keyName == "NUM_0" || keyName == "0") return Input::Key::NUM_0;
    if (keyName == "NUM_1" || keyName == "1") return Input::Key::NUM_1;
    if (keyName == "NUM_2" || keyName == "2") return Input::Key::NUM_2;
    if (keyName == "NUM_3" || keyName == "3") return Input::Key::NUM_3;
    if (keyName == "NUM_4" || keyName == "4") return Input::Key::NUM_4;
    if (keyName == "NUM_5" || keyName == "5") return Input::Key::NUM_5;
    if (keyName == "NUM_6" || keyName == "6") return Input::Key::NUM_6;
    if (keyName == "NUM_7" || keyName == "7") return Input::Key::NUM_7;
    if (keyName == "NUM_8" || keyName == "8") return Input::Key::NUM_8;
    if (keyName == "NUM_9" || keyName == "9") return Input::Key::NUM_9;

    // Special keys
    if (keyName == "SPACE") return Input::Key::SPACE;
    if (keyName == "ENTER") return Input::Key::ENTER;
    if (keyName == "ESC" || keyName == "ESCAPE") return Input::Key::ESC;
    if (keyName == "TAB") return Input::Key::TAB;
    if (keyName == "BACKSPACE") return Input::Key::BACKSPACE;
    if (keyName == "DELETE") return Input::Key::DELETE_;

    // Arrow keys
    if (keyName == "UP") return Input::Key::UP;
    if (keyName == "DOWN") return Input::Key::DOWN;
    if (keyName == "LEFT") return Input::Key::LEFT;
    if (keyName == "RIGHT") return Input::Key::RIGHT;

    // Function keys
    if (keyName == "F1") return Input::Key::F1;
    if (keyName == "F2") return Input::Key::F2;
    if (keyName == "F3") return Input::Key::F3;
    if (keyName == "F4") return Input::Key::F4;
    if (keyName == "F5") return Input::Key::F5;
    if (keyName == "F6") return Input::Key::F6;
    if (keyName == "F7") return Input::Key::F7;
    if (keyName == "F8") return Input::Key::F8;
    if (keyName == "F9") return Input::Key::F9;
    if (keyName == "F10") return Input::Key::F10;
    if (keyName == "F11") return Input::Key::F11;
    if (keyName == "F12") return Input::Key::F12;

    // Modifiers
    if (keyName == "SHIFT") return Input::Key::SHIFT;
    if (keyName == "CTRL" || keyName == "CONTROL") return Input::Key::CTRL;
    if (keyName == "ALT") return Input::Key::ALT;

    return Input::Key::UNKNOWN;
}

Input::MouseButton DesktopInputManager::ParseMouseButton(const std::string& buttonName) {
    if (buttonName == "LEFT") return Input::MouseButton::LEFT;
    if (buttonName == "RIGHT") return Input::MouseButton::RIGHT;
    if (buttonName == "MIDDLE") return Input::MouseButton::MIDDLE;
    if (buttonName == "BUTTON_4") return Input::MouseButton::BUTTON_4;
    if (buttonName == "BUTTON_5") return Input::MouseButton::BUTTON_5;

    return Input::MouseButton::UNKNOWN;
}

// ========== Editor Support ==========

void DesktopInputManager::SetGamePanelMousePos(float newX, float newY) {
    m_gamePanelMouseX = static_cast<double>(newX);
    m_gamePanelMouseY = static_cast<double>(newY);
}
