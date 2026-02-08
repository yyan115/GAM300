#include "pch.h"
#include "Input/AndroidInputManager.h"
#include "Platform/IPlatform.h"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/NameComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <WindowManager.hpp>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <iostream>
#include <cmath>

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
    LOGI("[AndroidInputManager] Initialized (entity-based with full touch tracking)");
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

glm::vec2 AndroidInputManager::GetActionTouchPosition(const std::string& action) {
    // Find the entity action and return its relative touch position
    for (const auto& entityAction : m_entityActions) {
        if (entityAction.actionName == action && entityAction.isPressed) {
            return entityAction.touchPositionRelative;
        }
    }
    return glm::vec2(0.0f);
}

glm::vec2 AndroidInputManager::GetAxis(const std::string& axisName) {
    // Movement axis: return normalized joystick direction (-1 to 1)
    if (axisName == "Movement") {
        for (const auto& entityAction : m_entityActions) {
            if (entityAction.actionName == "Movement" && entityAction.isPressed && entityAction.entityFound) {
                // Normalize by entity half-size to get -1 to 1 range
                float halfWidth = entityAction.entitySize.x / 2.0f;
                float halfHeight = entityAction.entitySize.y / 2.0f;

                if (halfWidth > 0.001f && halfHeight > 0.001f) {
                    float normX = entityAction.touchPositionRelative.x / halfWidth;
                    float normY = entityAction.touchPositionRelative.y / halfHeight;

                    // Clamp to -1 to 1
                    normX = glm::clamp(normX, -1.0f, 1.0f);
                    normY = glm::clamp(normY, -1.0f, 1.0f);

                    // Debug log periodically
                    static int moveLogCount = 0;
                    if (++moveLogCount % 30 == 1) {
                        LOGI("[AndroidInput] GetAxis(Movement) = (%.2f, %.2f) relPos=(%.1f,%.1f) halfSize=(%.1f,%.1f)",
                             normX, normY, entityAction.touchPositionRelative.x, entityAction.touchPositionRelative.y,
                             halfWidth, halfHeight);
                    }

                    return glm::vec2(normX, normY);
                }
            }
        }
        return glm::vec2(0.0f);
    }

    // Look axis: return drag delta from non-UI touches (for camera rotation)
    if (axisName == "Look") {
        // Return the delta from the drag touch (touch not on any UI entity)
        if (m_isDragging && m_dragTouchId != -1) {
            auto it = m_activeTouches.find(m_dragTouchId);
            if (it != m_activeTouches.end()) {
                // Return pixel delta - Lua script handles sensitivity
                // Normalize by viewport for consistent behavior across resolutions
                float viewportWidth = static_cast<float>(WindowManager::GetViewportWidth());
                float viewportHeight = static_cast<float>(WindowManager::GetViewportHeight());

                if (viewportWidth <= 0) viewportWidth = 1920.0f;
                if (viewportHeight <= 0) viewportHeight = 1080.0f;

                // Normalize delta to 0-1 range (relative to viewport)
                float normDeltaX = m_dragDelta.x / viewportWidth;
                float normDeltaY = m_dragDelta.y / viewportHeight;

                // Debug log periodically
                static int lookLogCount = 0;
                if ((normDeltaX != 0 || normDeltaY != 0) && ++lookLogCount % 30 == 1) {
                    LOGI("[AndroidInput] GetAxis(Look) = (%.4f, %.4f) dragDelta=(%.1f,%.1f)",
                         normDeltaX, normDeltaY, m_dragDelta.x, m_dragDelta.y);
                }

                return glm::vec2(normDeltaX, normDeltaY);
            }
        }
        return glm::vec2(0.0f);
    }

    return glm::vec2(0.0f);
}

bool AndroidInputManager::IsDragging() {
    return m_isDragging;
}

glm::vec2 AndroidInputManager::GetDragDelta() {
    return m_dragDelta;
}

bool AndroidInputManager::IsPointerPressed() {
    // Pointer = any non-handled touch
    for (const auto& [id, touch] : m_activeTouches) {
        if (!touch.isHandled) {
            return true;
        }
    }
    return false;
}

bool AndroidInputManager::IsPointerJustPressed() {
    // Check if any non-handled touch just began
    for (const auto& [id, touch] : m_activeTouches) {
        if (!touch.isHandled && touch.phase == TouchPhase::Began) {
            return true;
        }
    }
    return false;
}

glm::vec2 AndroidInputManager::GetPointerPosition() {
    // Return position of first non-handled touch
    for (const auto& [id, touch] : m_activeTouches) {
        if (!touch.isHandled) {
            return touch.position;
        }
    }
    return glm::vec2(0.0f);
}

int AndroidInputManager::GetTouchCount() {
    return static_cast<int>(m_activeTouches.size()) + static_cast<int>(m_endedTouches.size());
}

glm::vec2 AndroidInputManager::GetTouchPosition(int index) {
    if (index < 0) return glm::vec2(0.0f);

    // First check active touches
    if (index < static_cast<int>(m_activeTouches.size())) {
        auto it = m_activeTouches.begin();
        std::advance(it, index);
        return it->second.position;
    }

    // Then check ended touches
    int endedIndex = index - static_cast<int>(m_activeTouches.size());
    if (endedIndex < static_cast<int>(m_endedTouches.size())) {
        return m_endedTouches[endedIndex].position;
    }

    return glm::vec2(0.0f);
}

std::vector<InputManager::Touch> AndroidInputManager::GetTouches() {
    std::vector<Touch> result;

    // Add active touches
    for (const auto& [id, tp] : m_activeTouches) {
        Touch t;
        t.id = tp.id;
        t.phase = tp.phase;
        t.position = tp.position;
        t.startPosition = tp.startPosition;
        t.delta = tp.delta;
        t.entityName = tp.entityName;
        t.duration = tp.duration;
        result.push_back(t);
    }

    // Add ended touches (available for one frame)
    for (const auto& tp : m_endedTouches) {
        Touch t;
        t.id = tp.id;
        t.phase = TouchPhase::Ended;
        t.position = tp.position;
        t.startPosition = tp.startPosition;
        t.delta = tp.delta;
        t.entityName = tp.entityName;
        t.duration = tp.duration;
        result.push_back(t);
    }

    return result;
}

InputManager::Touch AndroidInputManager::GetTouchById(int touchId) {
    // Check active touches
    auto it = m_activeTouches.find(touchId);
    if (it != m_activeTouches.end()) {
        const auto& tp = it->second;
        Touch t;
        t.id = tp.id;
        t.phase = tp.phase;
        t.position = tp.position;
        t.startPosition = tp.startPosition;
        t.delta = tp.delta;
        t.entityName = tp.entityName;
        t.duration = tp.duration;
        return t;
    }

    // Check ended touches
    for (const auto& tp : m_endedTouches) {
        if (tp.id == touchId) {
            Touch t;
            t.id = tp.id;
            t.phase = TouchPhase::Ended;
            t.position = tp.position;
            t.startPosition = tp.startPosition;
            t.delta = tp.delta;
            t.entityName = tp.entityName;
            t.duration = tp.duration;
            return t;
        }
    }

    return Touch{};  // Return empty touch with id=-1
}

void AndroidInputManager::Update(float deltaTime) {
    m_currentTime += deltaTime;

    // Save previous state
    m_previousActions = m_currentActions;
    m_currentActions.clear();

    // Clear ended touches from last frame
    m_endedTouches.clear();

    // Reset drag delta (will be set in OnTouchMove if dragging)
    m_dragDelta = glm::vec2(0.0f);

    // Update entity transforms (look up from ECS)
    UpdateEntityTransforms();

    // Update touch phases and durations
    for (auto& [id, touch] : m_activeTouches) {
        touch.duration = m_currentTime - touch.startTime;

        // Transition Began -> Moved or Stationary
        // Keep Began for one full frame so IsPointerJustPressed works
        if (touch.phase == TouchPhase::Began) {
            if (touch.beganConsumed) {
                // Second Update call - now transition
                if (glm::length(touch.delta) > 0.001f) {
                    touch.phase = TouchPhase::Moved;
                } else {
                    touch.phase = TouchPhase::Stationary;
                }
            } else {
                touch.beganConsumed = true;
            }
        }
        // For subsequent frames, delta is reset each frame
        // Phase will be updated in OnTouchMove
    }

    // Update action states based on current touches
    for (auto& entityAction : m_entityActions) {
        if (entityAction.isPressed) {
            m_currentActions.insert(entityAction.actionName);
        }
    }

    // Detect gestures
    DetectGestures(deltaTime);

    // Reset delta for all touches (will be set in OnTouchMove)
    for (auto& [id, touch] : m_activeTouches) {
        // If not moved this frame, it's stationary
        if (touch.phase != TouchPhase::Began && touch.delta == glm::vec2(0.0f)) {
            touch.phase = TouchPhase::Stationary;
        }
        touch.delta = glm::vec2(0.0f);
    }
}

bool AndroidInputManager::LoadConfig(const std::string& path) {
    LOGI("[AndroidInputManager] Loading config from: %s", path.c_str());

    // Get platform instance
    IPlatform* platform = WindowManager::GetPlatform();
    if (!platform) {
        LOGE("[AndroidInputManager] ERROR: Platform is null!");
        return false;
    }

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

    // ===== Load Actions (Entity Bindings and Gestures) =====
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

            // Load entity binding (buttons, joysticks)
            if (androidBinding.HasMember("entity") && androidBinding["entity"].IsString()) {
                EntityAction entityAction;
                entityAction.actionName = actionName;
                entityAction.entityName = androidBinding["entity"].GetString();

                m_entityActions.push_back(entityAction);
                LOGI("[AndroidInputManager] Loaded entity action: %s -> %s",
                     actionName.c_str(), entityAction.entityName.c_str());
            }

            // Load gesture binding
            if (androidBinding.HasMember("gesture") && androidBinding["gesture"].IsObject()) {
                const auto& gestureData = androidBinding["gesture"];

                GestureBinding gesture;
                gesture.action = actionName;

                if (gestureData.HasMember("type") && gestureData["type"].IsString()) {
                    std::string type = gestureData["type"].GetString();

                    if (type == "swipe" || type == "swipe_right" || type == "swipe_left" ||
                        type == "swipe_up" || type == "swipe_down") {
                        gesture.type = GestureType::SWIPE;

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

    LOGI("[AndroidInputManager] Config loaded: %zu entity actions, %zu gestures",
         m_entityActions.size(), m_gestures.size());

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
    return {};
}

void AndroidInputManager::RenderOverlay(int screenWidth, int screenHeight) {
    // Entity-based system doesn't need to render overlays
}

// ========== Touch Event Handlers ==========

void AndroidInputManager::OnTouchDown(int pointerId, float x, float y) {
    // x, y are NORMALIZED (0-1) from AndroidPlatform::HandleTouchEvent
    // Convert to viewport pixel coords for consistency with desktop (ButtonSystem expects pixels)
    float viewportWidth = static_cast<float>(WindowManager::GetViewportWidth());
    float viewportHeight = static_cast<float>(WindowManager::GetViewportHeight());

    // Fallback if viewport not ready
    if (viewportWidth <= 0) viewportWidth = 1920.0f;
    if (viewportHeight <= 0) viewportHeight = 1080.0f;

    glm::vec2 pixelPos(x * viewportWidth, y * viewportHeight);

    TouchPoint touch;
    touch.id = pointerId;
    touch.position = pixelPos;  // Store as pixels for GetPointerPosition compatibility
    touch.startPosition = pixelPos;
    touch.previousPosition = pixelPos;
    touch.delta = glm::vec2(0.0f);
    touch.startTime = GetCurrentTime();
    touch.duration = 0.0f;
    touch.phase = TouchPhase::Began;
    touch.entityName = "";
    touch.isHandled = false;

    // Convert pixel coordinates to game coordinates for entity hit testing
    int gameResWidth, gameResHeight;
    GraphicsManager::GetInstance().GetTargetGameResolution(gameResWidth, gameResHeight);

    // pixel -> game coordinates, with Y flipped (screen Y=0 is top, game Y=0 is bottom)
    float gameX = (pixelPos.x / viewportWidth) * static_cast<float>(gameResWidth);
    float gameY = static_cast<float>(gameResHeight) - (pixelPos.y / viewportHeight) * static_cast<float>(gameResHeight);
    glm::vec2 gamePos(gameX, gameY);

    LOGI("[AndroidInput] TouchDown id=%d norm=(%.3f,%.3f) pixel=(%.1f,%.1f) game=(%.1f,%.1f) viewport=(%.0f,%.0f) gameRes=(%d,%d)",
         pointerId, x, y, pixelPos.x, pixelPos.y, gameX, gameY, viewportWidth, viewportHeight, gameResWidth, gameResHeight);

    // Check if touch hits any entity action
    for (auto& entityAction : m_entityActions) {
        if (!entityAction.entityFound) {
            LOGI("[AndroidInput]   Skipping '%s' - entity not found", entityAction.entityName.c_str());
            continue;
        }
        if (entityAction.isPressed) {
            LOGI("[AndroidInput]   Skipping '%s' - already pressed", entityAction.entityName.c_str());
            continue;
        }

        // Check if touch is inside entity bounds
        float halfWidth = entityAction.entitySize.x / 2.0f;
        float halfHeight = entityAction.entitySize.y / 2.0f;
        float minX = entityAction.entityCenter.x - halfWidth;
        float maxX = entityAction.entityCenter.x + halfWidth;
        float minY = entityAction.entityCenter.y - halfHeight;
        float maxY = entityAction.entityCenter.y + halfHeight;

        LOGI("[AndroidInput]   Checking '%s': bounds=(%.1f,%.1f)-(%.1f,%.1f) touch=(%.1f,%.1f)",
             entityAction.entityName.c_str(), minX, minY, maxX, maxY, gamePos.x, gamePos.y);

        if (gamePos.x >= minX && gamePos.x <= maxX &&
            gamePos.y >= minY && gamePos.y <= maxY) {

            entityAction.isPressed = true;
            entityAction.activeTouchId = pointerId;
            entityAction.touchPositionRelative = gamePos - entityAction.entityCenter;

            touch.isHandled = true;
            touch.entityName = entityAction.entityName;

            LOGI("[AndroidInput] HIT! Touch %d on entity '%s' for action '%s' relPos=(%.1f,%.1f)",
                 pointerId, entityAction.entityName.c_str(), entityAction.actionName.c_str(),
                 entityAction.touchPositionRelative.x, entityAction.touchPositionRelative.y);
            break;
        }
    }

    // If touch wasn't handled by any entity, it's available for camera/other use
    if (!touch.isHandled) {
        if (m_dragTouchId == -1) {
            m_dragTouchId = pointerId;
            m_isDragging = true;
            LOGI("[AndroidInput] Touch %d BEGAN - no entity hit, using for camera drag", pointerId);
        }
    }

    m_activeTouches[pointerId] = touch;
}

void AndroidInputManager::OnTouchMove(int pointerId, float x, float y) {
    auto it = m_activeTouches.find(pointerId);
    if (it == m_activeTouches.end()) return;

    // x, y are NORMALIZED (0-1) - convert to pixel coords
    float viewportWidth = static_cast<float>(WindowManager::GetViewportWidth());
    float viewportHeight = static_cast<float>(WindowManager::GetViewportHeight());

    if (viewportWidth <= 0) viewportWidth = 1920.0f;
    if (viewportHeight <= 0) viewportHeight = 1080.0f;

    glm::vec2 pixelPos(x * viewportWidth, y * viewportHeight);
    glm::vec2 previousPos = it->second.position;

    it->second.delta = pixelPos - previousPos;
    it->second.previousPosition = it->second.position;
    it->second.position = pixelPos;
    it->second.phase = TouchPhase::Moved;

    // Convert pixel to game coordinates
    int gameResWidth, gameResHeight;
    GraphicsManager::GetInstance().GetTargetGameResolution(gameResWidth, gameResHeight);

    float gameX = (pixelPos.x / viewportWidth) * static_cast<float>(gameResWidth);
    float gameY = static_cast<float>(gameResHeight) - (pixelPos.y / viewportHeight) * static_cast<float>(gameResHeight);
    glm::vec2 gamePos(gameX, gameY);

    // Update entity action touch positions
    for (auto& entityAction : m_entityActions) {
        if (entityAction.isPressed && entityAction.activeTouchId == pointerId) {
            entityAction.touchPositionRelative = gamePos - entityAction.entityCenter;
            break;
        }
    }

    // Update drag delta if this is the drag touch
    if (pointerId == m_dragTouchId) {
        m_dragDelta = it->second.delta;
    }
}

void AndroidInputManager::OnTouchUp(int pointerId, float x, float y) {
    auto it = m_activeTouches.find(pointerId);
    if (it == m_activeTouches.end()) return;

    // Copy touch to ended list before removing
    TouchPoint endedTouch = it->second;
    endedTouch.phase = TouchPhase::Ended;
    endedTouch.duration = m_currentTime - endedTouch.startTime;
    m_endedTouches.push_back(endedTouch);

    LOGI("[AndroidInputManager] Touch %d ENDED on entity '%s' (duration: %.2fs)",
         pointerId, endedTouch.entityName.c_str(), endedTouch.duration);

    // Release entity actions
    for (auto& entityAction : m_entityActions) {
        if (entityAction.activeTouchId == pointerId) {
            entityAction.isPressed = false;
            entityAction.activeTouchId = -1;
            entityAction.touchPositionRelative = glm::vec2(0.0f);
        }
    }

    // Release camera drag
    if (pointerId == m_dragTouchId) {
        m_dragTouchId = -1;
        m_isDragging = false;
        m_dragDelta = glm::vec2(0.0f);
    }

    // Remove from active touches
    m_activeTouches.erase(pointerId);
}

// ========== Private Helper Methods ==========

void AndroidInputManager::UpdateEntityTransforms() {
    ECSManager* ecs = nullptr;
    try {
        ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    } catch (...) {
        return;
    }

    if (!ecs) return;

    const auto& entities = ecs->GetActiveEntities();

    // Debug: log entity search periodically
    static int entityLogCount = 0;
    bool shouldLog = (++entityLogCount % 300 == 1);  // Log every ~5 seconds at 60fps

    for (auto& entityAction : m_entityActions) {
        bool wasFound = entityAction.entityFound;
        entityAction.entityFound = false;

        for (Entity e : entities) {
            if (!ecs->HasComponent<NameComponent>(e)) continue;

            auto& nameComp = ecs->GetComponent<NameComponent>(e);
            if (nameComp.name != entityAction.entityName) continue;

            if (!ecs->HasComponent<Transform>(e)) continue;

            auto& transform = ecs->GetComponent<Transform>(e);

            entityAction.entityCenter = glm::vec2(transform.localPosition.x, transform.localPosition.y);
            entityAction.entitySize = glm::vec2(transform.localScale.x, transform.localScale.y);
            entityAction.entityFound = true;

            // Log when entity is first found or periodically
            if (!wasFound || shouldLog) {
                LOGI("[AndroidInput] Entity '%s' for action '%s': center=(%.1f,%.1f) size=(%.1f,%.1f)",
                     entityAction.entityName.c_str(), entityAction.actionName.c_str(),
                     entityAction.entityCenter.x, entityAction.entityCenter.y,
                     entityAction.entitySize.x, entityAction.entitySize.y);
            }
            break;
        }

        // Log if entity not found
        if (!entityAction.entityFound && wasFound) {
            LOGI("[AndroidInput] Entity '%s' for action '%s' NOT FOUND",
                 entityAction.entityName.c_str(), entityAction.actionName.c_str());
        }
    }
}

bool AndroidInputManager::IsTouchInsideEntity(const EntityAction& entity, glm::vec2 touchPos) {
    if (!entity.entityFound) return false;

    float halfWidth = entity.entitySize.x / 2.0f;
    float halfHeight = entity.entitySize.y / 2.0f;
    float minX = entity.entityCenter.x - halfWidth;
    float maxX = entity.entityCenter.x + halfWidth;
    float minY = entity.entityCenter.y - halfHeight;
    float maxY = entity.entityCenter.y + halfHeight;

    return touchPos.x >= minX && touchPos.x <= maxX &&
           touchPos.y >= minY && touchPos.y <= maxY;
}

void AndroidInputManager::DetectGestures(float deltaTime) {
    DetectSwipes();
    DetectDoubleTap();
}

void AndroidInputManager::DetectSwipes() {
    for (const auto& gesture : m_gestures) {
        if (gesture.type != GestureType::SWIPE) continue;
        // TODO: Implement swipe detection using ended touches
    }
}

void AndroidInputManager::DetectDoubleTap() {
    for (const auto& gesture : m_gestures) {
        if (gesture.type != GestureType::DOUBLE_TAP) continue;

        for (const auto& [id, touch] : m_activeTouches) {
            if (touch.phase == TouchPhase::Began) {
                float timeSinceLastTap = m_currentTime - m_lastTapTime;

                if (timeSinceLastTap < gesture.maxTimeBetweenTaps) {
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

float AndroidInputManager::GetCurrentTime() {
    return m_currentTime;
}

void AndroidInputManager::SetGamePanelMousePos(float newX, float newY) {
    (void)newX;
    (void)newY;
}
