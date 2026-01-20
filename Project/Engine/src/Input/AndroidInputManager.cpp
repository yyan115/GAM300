#include "pch.h"
#include "Input/AndroidInputManager.h"
#include "Platform/IPlatform.h"
#include <WindowManager.hpp>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <iostream>
#include <cmath>
#include <chrono>

#ifdef ANDROID
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "GAM300", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GAM300", __VA_ARGS__)
#else
#define LOGI(...) printf(__VA_ARGS__); printf("\n")
#define LOGE(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
#endif

// ========== Constructor ==========

AndroidInputManager::AndroidInputManager() {
    LOGI("[AndroidInputManager] Initialized");
}

// ========== IInputSystem Interface Implementation ==========

bool AndroidInputManager::IsActionPressed(const std::string& action) {
    return m_currentActions.count(action) > 0;
}

bool AndroidInputManager::IsActionJustPressed(const std::string& action) {
    return m_currentActions.count(action) > 0 &&
           m_previousActions.count(action) == 0;
}

bool AndroidInputManager::IsActionJustReleased(const std::string& action) {
    return m_currentActions.count(action) == 0 &&
           m_previousActions.count(action) > 0;
}

glm::vec2 AndroidInputManager::GetAxis(const std::string& axisName) {
    // Check joysticks
    for (const auto& joystick : m_joysticks) {
        if (joystick.axisName == axisName) {
            return joystick.normalizedValue;
        }
    }

    // Check drag zones (for camera look)
    for (const auto& dragZone : m_dragZones) {
        if (dragZone.axisName == axisName) {
            return dragZone.delta * dragZone.sensitivity;
        }
    }

    return glm::vec2(0.0f);
}

bool AndroidInputManager::IsPointerPressed() {
    // Pointer = primary touch (first non-consumed touch)
    for (const auto& [id, touch] : m_activeTouches) {
        if (!touch.consumed) {
            return true;
        }
    }
    return false;
}

bool AndroidInputManager::IsPointerJustPressed() {
    // Check if any non-consumed touch was just pressed
    for (const auto& [id, touch] : m_activeTouches) {
        if (!touch.consumed) {
            // Check if this touch is new (started this frame)
            float timeSinceStart = m_currentTime - touch.startTime;
            if (timeSinceStart < 0.05f) {  // Within 50ms = just pressed
                return true;
            }
        }
    }
    return false;
}

glm::vec2 AndroidInputManager::GetPointerPosition() {
    // Return position of first non-consumed touch
    for (const auto& [id, touch] : m_activeTouches) {
        if (!touch.consumed) {
            return touch.position;
        }
    }
    return glm::vec2(0.0f);
}

int AndroidInputManager::GetTouchCount() {
    return static_cast<int>(m_activeTouches.size());
}

glm::vec2 AndroidInputManager::GetTouchPosition(int index) {
    if (index < 0 || index >= static_cast<int>(m_activeTouches.size())) {
        return glm::vec2(0.0f);
    }

    auto it = m_activeTouches.begin();
    std::advance(it, index);
    return it->second.position;
}

void AndroidInputManager::Update(float deltaTime) {
    m_currentTime += deltaTime;

    // Save previous state
    m_previousActions = m_currentActions;
    m_currentActions.clear();

    // Update all virtual controls
    UpdateTouchZones();
    UpdateJoysticks();
    UpdateDragZones();

    // Detect gestures
    DetectGestures(deltaTime);
}

bool AndroidInputManager::LoadConfig(const std::string& path) {
    LOGI("[AndroidInputManager] Loading config from: %s", path.c_str());

    // Get platform instance
    IPlatform* platform = WindowManager::GetPlatform();
    if (!platform) {
        LOGE("[AndroidInputManager] ERROR: Platform is null!");
        return false;
    }

    // Try to read directly without FileExists check (FileExists may have issues)
    // Log what we're attempting
    LOGI("[AndroidInputManager] Attempting to read config file directly...");

    // Load file using platform
    std::vector<uint8_t> configData = platform->ReadAsset(path);
    if (configData.empty()) {
        LOGE("[AndroidInputManager] ERROR: Failed to read config file: %s", path.c_str());
        return false;
    }

    LOGI("[AndroidInputManager] Config file loaded, size: %zu bytes", configData.size());

    // Parse JSON
    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(configData.data()), configData.size());

    if (doc.HasParseError()) {
        LOGE("[AndroidInputManager] ERROR: JSON parse error at offset %zu: %s",
             doc.GetErrorOffset(), rapidjson::GetParseError_En(doc.GetParseError()));
        return false;
    }

    // ===== Load Actions (Touch Zones and Gestures) =====
    // New format: actions at root level, with "android" nested inside each action
    if (doc.HasMember("actions") && doc["actions"].IsObject()) {
        const auto& actions = doc["actions"];

        for (auto it = actions.MemberBegin(); it != actions.MemberEnd(); ++it) {
            std::string actionName = it->name.GetString();
            const auto& actionData = it->value;

            // Skip if no android binding
            if (!actionData.HasMember("android") || !actionData["android"].IsObject()) {
                LOGI("[AndroidInputManager] Skipping action '%s' (no android binding)", actionName.c_str());
                continue;
            }

            const auto& androidBinding = actionData["android"];

            // Load touch zone (single object, not array)
            if (androidBinding.HasMember("touchZone") && androidBinding["touchZone"].IsObject()) {
                const auto& zoneData = androidBinding["touchZone"];

                TouchZone zone;
                zone.action = actionName;

                // Parse position
                if (zoneData.HasMember("x") && zoneData.HasMember("y")) {
                    zone.position.x = zoneData["x"].GetFloat();
                    zone.position.y = zoneData["y"].GetFloat();
                }

                // Parse size - determine circle vs rect based on presence of radius
                if (zoneData.HasMember("radius")) {
                    zone.isCircle = true;
                    zone.radius = zoneData["radius"].GetFloat();
                } else if (zoneData.HasMember("width") && zoneData.HasMember("height")) {
                    zone.isCircle = false;
                    zone.rectSize.x = zoneData["width"].GetFloat();
                    zone.rectSize.y = zoneData["height"].GetFloat();
                }

                // Parse visuals (optional)
                if (zoneData.HasMember("normalImage") && zoneData["normalImage"].IsString()) {
                    zone.normalImage = zoneData["normalImage"].GetString();
                }
                if (zoneData.HasMember("pressedImage") && zoneData["pressedImage"].IsString()) {
                    zone.pressedImage = zoneData["pressedImage"].GetString();
                }
                if (zoneData.HasMember("alpha")) {
                    zone.alpha = zoneData["alpha"].GetFloat();
                }

                m_touchZones.push_back(zone);
                LOGI("[AndroidInputManager] Loaded touch zone for action: %s", actionName.c_str());
            }

            // Load gesture (single object, not array)
            if (androidBinding.HasMember("gesture") && androidBinding["gesture"].IsObject()) {
                const auto& gestureData = androidBinding["gesture"];

                GestureBinding gesture;
                gesture.action = actionName;

                if (gestureData.HasMember("type") && gestureData["type"].IsString()) {
                    std::string type = gestureData["type"].GetString();

                    if (type == "swipe" || type == "swipe_right" || type == "swipe_left" ||
                        type == "swipe_up" || type == "swipe_down") {
                        gesture.type = GestureType::SWIPE;

                        // Parse direction
                        if (type == "swipe_right") gesture.direction = glm::vec2(1.0f, 0.0f);
                        else if (type == "swipe_left") gesture.direction = glm::vec2(-1.0f, 0.0f);
                        else if (type == "swipe_up") gesture.direction = glm::vec2(0.0f, -1.0f);
                        else if (type == "swipe_down") gesture.direction = glm::vec2(0.0f, 1.0f);

                        if (gestureData.HasMember("minDistance")) {
                            gesture.minDistance = gestureData["minDistance"].GetFloat();
                        }
                        if (gestureData.HasMember("maxTime")) {
                            gesture.maxTime = gestureData["maxTime"].GetFloat();
                        }
                    }
                    else if (type == "double_tap") {
                        gesture.type = GestureType::DOUBLE_TAP;

                        if (gestureData.HasMember("maxInterval")) {
                            gesture.maxTimeBetweenTaps = gestureData["maxInterval"].GetFloat();
                        }
                    }
                }

                m_gestures.push_back(gesture);
                LOGI("[AndroidInputManager] Loaded gesture for action: %s", actionName.c_str());
            }
        }
    }

    // ===== Load Axes (Joysticks and Drag Zones) =====
    // New format: axes at root level, with "android" nested inside each axis
    if (doc.HasMember("axes") && doc["axes"].IsObject()) {
        const auto& axes = doc["axes"];

        for (auto it = axes.MemberBegin(); it != axes.MemberEnd(); ++it) {
            std::string axisName = it->name.GetString();
            const auto& axisData = it->value;

            // Skip if no android binding
            if (!axisData.HasMember("android") || !axisData["android"].IsObject()) {
                LOGI("[AndroidInputManager] Skipping axis '%s' (no android binding)", axisName.c_str());
                continue;
            }

            const auto& androidBinding = axisData["android"];

            if (!androidBinding.HasMember("type") || !androidBinding["type"].IsString()) {
                continue;
            }

            std::string type = androidBinding["type"].GetString();

            if (type == "virtual_joystick") {
                VirtualJoystick joystick;
                joystick.axisName = axisName;

                // Parse position (new format uses nested "position" object)
                if (androidBinding.HasMember("position") && androidBinding["position"].IsObject()) {
                    const auto& pos = androidBinding["position"];
                    if (pos.HasMember("x")) joystick.basePosition.x = pos["x"].GetFloat();
                    if (pos.HasMember("y")) joystick.basePosition.y = pos["y"].GetFloat();
                }

                // Parse radii
                if (androidBinding.HasMember("outerRadius")) {
                    joystick.outerRadius = androidBinding["outerRadius"].GetFloat();
                }
                if (androidBinding.HasMember("innerRadius")) {
                    joystick.innerRadius = androidBinding["innerRadius"].GetFloat();
                }
                if (androidBinding.HasMember("deadZone")) {
                    joystick.deadZone = androidBinding["deadZone"].GetFloat();
                } else {
                    joystick.deadZone = 0.1f;  // Default
                }

                // Parse visuals (optional)
                if (androidBinding.HasMember("outerImage") && androidBinding["outerImage"].IsString()) {
                    joystick.outerImage = androidBinding["outerImage"].GetString();
                }
                if (androidBinding.HasMember("innerImage") && androidBinding["innerImage"].IsString()) {
                    joystick.innerImage = androidBinding["innerImage"].GetString();
                }
                if (androidBinding.HasMember("alpha")) {
                    joystick.alpha = androidBinding["alpha"].GetFloat();
                }

                m_joysticks.push_back(joystick);
                LOGI("[AndroidInputManager] Loaded virtual joystick: %s", axisName.c_str());
            }
            else if (type == "touch_drag") {
                TouchDragZone dragZone;
                dragZone.axisName = axisName;

                // Parse zone
                if (androidBinding.HasMember("zone") && androidBinding["zone"].IsObject()) {
                    const auto& zone = androidBinding["zone"];

                    if (zone.HasMember("x")) dragZone.zonePosition.x = zone["x"].GetFloat();
                    if (zone.HasMember("y")) dragZone.zonePosition.y = zone["y"].GetFloat();
                    if (zone.HasMember("width")) dragZone.zoneSize.x = zone["width"].GetFloat();
                    if (zone.HasMember("height")) dragZone.zoneSize.y = zone["height"].GetFloat();
                }

                // Parse sensitivity
                if (androidBinding.HasMember("sensitivity")) {
                    dragZone.sensitivity = androidBinding["sensitivity"].GetFloat();
                } else {
                    dragZone.sensitivity = 1.0f;
                }

                m_dragZones.push_back(dragZone);
                LOGI("[AndroidInputManager] Loaded touch drag zone: %s", axisName.c_str());
            }
        }
    }

    LOGI("[AndroidInputManager] Config loaded: %zu touch zones, %zu joysticks, %zu drag zones, %zu gestures",
         m_touchZones.size(), m_joysticks.size(), m_dragZones.size(), m_gestures.size());

    return true;
}

std::unordered_map<std::string, bool> AndroidInputManager::GetAllActionStates() {
    std::unordered_map<std::string, bool> states;

    for (const auto& action : m_currentActions) {
        states[action] = true;
    }

    return states;
}

std::unordered_map<std::string, glm::vec2> AndroidInputManager::GetAllAxisStates() {
    std::unordered_map<std::string, glm::vec2> states;

    for (const auto& joystick : m_joysticks) {
        states[joystick.axisName] = joystick.normalizedValue;
    }

    for (const auto& dragZone : m_dragZones) {
        states[dragZone.axisName] = dragZone.delta * dragZone.sensitivity;
    }

    return states;
}

void AndroidInputManager::RenderOverlay(int screenWidth, int screenHeight) {
    // TODO: Implement rendering using your rendering system
    // This is a placeholder showing what needs to be rendered

    // Render touch zones
    for (const auto& zone : m_touchZones) {
        glm::vec2 screenPos(
            zone.position.x * screenWidth,
            zone.position.y * screenHeight
        );

        const std::string& imagePath = zone.isPressed ? zone.pressedImage : zone.normalImage;

        // TODO: Call your renderer
        // Renderer::DrawSprite(imagePath, screenPos, zone.radius * screenWidth, zone.alpha);
    }

    // Render joysticks
    for (const auto& joystick : m_joysticks) {
        glm::vec2 screenPos(
            joystick.basePosition.x * screenWidth,
            joystick.basePosition.y * screenHeight
        );

        // Draw outer circle
        // TODO: Renderer::DrawSprite(joystick.outerImage, screenPos, outerRadius * screenWidth, alpha);

        // Draw inner stick
        if (joystick.isActive) {
            glm::vec2 stickScreenPos(
                (joystick.basePosition.x + joystick.stickOffset.x) * screenWidth,
                (joystick.basePosition.y + joystick.stickOffset.y) * screenHeight
            );
            // TODO: Renderer::DrawSprite(joystick.innerImage, stickScreenPos, innerRadius * screenWidth, alpha);
        }
    }
}

// ========== Touch Event Handlers ==========

void AndroidInputManager::OnTouchDown(int pointerId, float x, float y) {
    glm::vec2 normalizedPos(x, y);

    TouchPoint touch;
    touch.id = pointerId;
    touch.position = normalizedPos;
    touch.startPosition = normalizedPos;
    touch.startTime = GetCurrentTime();
    touch.consumed = false;

    // Check if touch hits any zone (priority: zones > joysticks > drag zones)
    for (auto& zone : m_touchZones) {
        if (IsTouchInsideZone(zone, normalizedPos)) {
            zone.isPressed = true;
            zone.activeTouchId = pointerId;
            touch.consumed = true;
            m_currentActions.insert(zone.action);
            break;  // Only one zone per touch
        }
    }

    // Check if touch hits joystick
    if (!touch.consumed) {
        for (auto& joystick : m_joysticks) {
            float dist = glm::distance(joystick.basePosition, normalizedPos);
            if (dist <= joystick.outerRadius) {
                joystick.isActive = true;
                joystick.activeTouchId = pointerId;
                touch.consumed = true;

                // Calculate initial offset on touch down (not just on move)
                glm::vec2 offset = normalizedPos - joystick.basePosition;
                float distance = glm::length(offset);

                if (distance < joystick.deadZone) {
                    joystick.normalizedValue = glm::vec2(0.0f);
                    joystick.stickOffset = glm::vec2(0.0f);
                } else {
                    if (distance > joystick.outerRadius) {
                        offset = glm::normalize(offset) * joystick.outerRadius;
                        distance = joystick.outerRadius;
                    }
                    joystick.normalizedValue = offset / joystick.outerRadius;
                    joystick.stickOffset = offset;
                }
                break;
            }
        }
    }

    // Check if touch is in drag zone
    if (!touch.consumed) {
        for (auto& dragZone : m_dragZones) {
            if (IsPointInRect(normalizedPos, dragZone.zonePosition, dragZone.zoneSize)) {
                LOGI("[AndroidInputManager] Touch in drag zone '%s' at (%.2f, %.2f)",
                     dragZone.axisName.c_str(), normalizedPos.x, normalizedPos.y);
                dragZone.isActive = true;
                dragZone.activeTouchId = pointerId;
                dragZone.previousPosition = normalizedPos;
                touch.consumed = true;
                break;
            }
        }
    }

    m_activeTouches[pointerId] = touch;
}

void AndroidInputManager::OnTouchMove(int pointerId, float x, float y) {
    auto it = m_activeTouches.find(pointerId);
    if (it == m_activeTouches.end()) return;

    glm::vec2 normalizedPos(x, y);
    it->second.position = normalizedPos;

    // Update joysticks
    for (auto& joystick : m_joysticks) {
        if (joystick.isActive && joystick.activeTouchId == pointerId) {
            // Calculate offset from joystick center
            glm::vec2 offset = normalizedPos - joystick.basePosition;
            float distance = glm::length(offset);

            // Apply dead zone
            if (distance < joystick.deadZone) {
                joystick.normalizedValue = glm::vec2(0.0f);
                joystick.stickOffset = glm::vec2(0.0f);
            } else {
                // Clamp to outer radius
                if (distance > joystick.outerRadius) {
                    offset = glm::normalize(offset) * joystick.outerRadius;
                    distance = joystick.outerRadius;
                }

                // Normalize to -1..1 range
                joystick.normalizedValue = offset / joystick.outerRadius;
                joystick.stickOffset = offset;
            }
            break;
        }
    }

    // Update drag zones
    for (auto& dragZone : m_dragZones) {
        if (dragZone.isActive && dragZone.activeTouchId == pointerId) {
            dragZone.delta = normalizedPos - dragZone.previousPosition;
            dragZone.previousPosition = normalizedPos;
            dragZone.movedThisFrame = true;
            // Log non-zero deltas
            if (dragZone.delta.x != 0.0f || dragZone.delta.y != 0.0f) {
                LOGI("[AndroidInputManager] Drag '%s' delta: (%.4f, %.4f)",
                     dragZone.axisName.c_str(), dragZone.delta.x, dragZone.delta.y);
            }
            break;
        }
    }
}

void AndroidInputManager::OnTouchUp(int pointerId, float x, float y) {
    // Release touch zones
    for (auto& zone : m_touchZones) {
        if (zone.activeTouchId == pointerId) {
            zone.isPressed = false;
            zone.activeTouchId = -1;
            m_currentActions.erase(zone.action);
        }
    }

    // Release joysticks
    for (auto& joystick : m_joysticks) {
        if (joystick.activeTouchId == pointerId) {
            joystick.isActive = false;
            joystick.activeTouchId = -1;
            joystick.normalizedValue = glm::vec2(0.0f);
            joystick.stickOffset = glm::vec2(0.0f);
        }
    }

    // Release drag zones
    for (auto& dragZone : m_dragZones) {
        if (dragZone.activeTouchId == pointerId) {
            dragZone.isActive = false;
            dragZone.activeTouchId = -1;
            dragZone.delta = glm::vec2(0.0f);
        }
    }

    // Remove from active touches
    m_activeTouches.erase(pointerId);
}

// ========== Private Helper Methods ==========

void AndroidInputManager::UpdateTouchZones() {
    // Touch zones are updated in OnTouchDown/OnTouchUp
    // This method is for any per-frame logic
}

void AndroidInputManager::UpdateJoysticks() {
    // Joysticks are updated in OnTouchMove
    // Reset delta for inactive joysticks
    for (auto& joystick : m_joysticks) {
        if (!joystick.isActive) {
            joystick.normalizedValue = glm::vec2(0.0f);
        }
    }
}

void AndroidInputManager::UpdateDragZones() {
    // Reset delta for drag zones that didn't move this frame
    // This ensures delta is 0 when finger is stationary
    for (auto& dragZone : m_dragZones) {
        if (!dragZone.movedThisFrame) {
            dragZone.delta = glm::vec2(0.0f);
        }
        // Reset the flag for next frame
        dragZone.movedThisFrame = false;
    }
}

void AndroidInputManager::DetectGestures(float deltaTime) {
    DetectSwipes();
    DetectDoubleTap();
}

void AndroidInputManager::DetectSwipes() {
    // Check each touch that just ended
    // (In a real implementation, you'd track touches ending this frame)
    // For now, this is a simplified version

    for (const auto& gesture : m_gestures) {
        if (gesture.type != GestureType::SWIPE) continue;

        // Check all ending touches
        // TODO: Implement proper swipe detection with touch tracking
    }
}

void AndroidInputManager::DetectDoubleTap() {
    for (const auto& gesture : m_gestures) {
        if (gesture.type != GestureType::DOUBLE_TAP) continue;

        // Check if any touch just started
        for (const auto& [id, touch] : m_activeTouches) {
            float timeSinceStart = m_currentTime - touch.startTime;
            if (timeSinceStart < 0.05f) {  // Just started
                float timeSinceLastTap = m_currentTime - m_lastTapTime;

                if (timeSinceLastTap < gesture.maxTimeBetweenTaps) {
                    // Double tap detected!
                    m_currentActions.insert(gesture.action);
                    m_tapCount = 0;
                } else {
                    m_tapCount = 1;
                    m_lastTapTime = m_currentTime;
                    m_lastTapPosition = touch.position;
                }
            }
        }
    }
}

bool AndroidInputManager::IsTouchInsideZone(const TouchZone& zone, glm::vec2 touchPos) {
    if (zone.isCircle) {
        float dist = glm::distance(zone.position, touchPos);
        return dist <= zone.radius;
    } else {
        return IsPointInRect(touchPos, zone.position - zone.rectSize * 0.5f, zone.rectSize);
    }
}

bool AndroidInputManager::IsPointInRect(glm::vec2 point, glm::vec2 rectPos, glm::vec2 rectSize) {
    return point.x >= rectPos.x &&
           point.x <= rectPos.x + rectSize.x &&
           point.y >= rectPos.y &&
           point.y <= rectPos.y + rectSize.y;
}

float AndroidInputManager::GetCurrentTime() {
    return m_currentTime;
}

// ========== Editor Support ==========

void AndroidInputManager::SetGamePanelMousePos(float newX, float newY) {
    // Not used on Android
    (void)newX;
    (void)newY;
}
