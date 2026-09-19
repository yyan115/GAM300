#include "pch.h"
#include "UI/QuitConfirmation.hpp"

#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "Prefab/PrefabIO.hpp"
#include "Platform/IPlatform.h"
#include "Sound/AudioManager.hpp"
#include "TimeManager.hpp"
#include "WindowManager.hpp"
#include "Logging.hpp"

#include <chrono>
#include <optional>

namespace {
    // Written the way the scenes write prefab paths, which resolves from the
    // editor's working directory as well as the game's and Android's.
    constexpr const char* kPromptPrefab = "../../Resources/Prefabs/QuitPromptUI.prefab";

    enum class State { Idle, Prompting, Closing };

    State g_state = State::Idle;

    // An entity handle together with the GUID it had, which is unique to one
    // instantiation. Entity ids are reused once freed, so the handle is only
    // trusted while the GUID still names it: that stays true until the entity
    // is destroyed, by us or by its scene unloading.
    struct Handle {
        Entity entity = INVALID_ENTITY;
        GUID_128 guid{};

        static Handle Of(Entity e) {
            return { e, EntityGUIDRegistry::GetInstance().GetGUIDByEntity(e) };
        }
        bool Alive() const {
            return entity != INVALID_ENTITY
                && EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid) == entity;
        }
    };

    // The prompt on screen.
    Handle g_prompt;
    unsigned g_framesShown = 0;

    // A dismissed prompt, removed at the start of the next frame rather than
    // inside the button press that dismissed it, which is still iterating it.
    Handle g_dismissed;

    // A held Alt+F4 sends the close request again at the key repeat rate, and
    // a repeat is not an answer to the prompt it opened. How a repeat is told
    // from a second press depends on whether the game sees the key.
    //
    // Windows passes F4 to the window along with the close, so the key is
    // down when the prompt opens. Then a request counts once F4 has been let
    // go since, however soon after.
    //
    // A Linux desktop that owns the shortcut never passes F4 on. Then a
    // request that follows the previous one within kRepeatWindow is a repeat.
    // The first repeat comes after the desktop's repeat delay, 600 ms by
    // default on KDE and 660 ms on X, and each repeat restarts the window.
    using Clock = std::chrono::steady_clock;
    constexpr std::chrono::milliseconds kRepeatWindow{1000};
    std::optional<Clock::time_point> g_lastWindowRequest;
    bool g_closeKeySeen = false;       // F4 was down when the prompt opened
    bool g_awaitCloseKeyRelease = false;

    bool IsCloseKeyHeld() {
        IPlatform* platform = WindowManager::GetPlatform();
        return platform && platform->IsKeyPressed(Input::Key::F4);
    }

    bool g_rebaselineInput = false;

    // Whether the game's time and sound are held for the prompt.
    bool g_held = false;

    void Hold(bool held) {
        if (held == g_held) return;
        g_held = held;
        AudioManager::GetInstance().SetModalSuspended(held);
        TimeManager::SetFrozen(held);
    }

    void BeginPrompting() {
        g_state = State::Prompting;
        Hold(true);
    }

    void ForgetPrompt() {
        g_prompt = Handle{};
        g_framesShown = 0;
    }
}

void QuitConfirmation::OnWindowCloseRequest() {
    const Clock::time_point now = Clock::now();
    const bool withinRepeatWindow =
        g_lastWindowRequest && now - *g_lastWindowRequest < kRepeatWindow;
    g_lastWindowRequest = now;

    switch (g_state) {
    case State::Idle:
        if (!WindowManager::IsWindowFocused()) {
            Confirm();
            return;
        }
        BeginPrompting();
        g_closeKeySeen = IsCloseKeyHeld();
        g_awaitCloseKeyRelease = g_closeKeySeen;
        return;
    case State::Prompting: {
        const bool repeat = g_closeKeySeen ? g_awaitCloseKeyRelease : withinRepeatWindow;
        if (repeat) return;
        Confirm();
        return;
    }
    case State::Closing:
        return;
    }
}

void QuitConfirmation::Request() {
    if (g_state == State::Idle) {
        BeginPrompting();
        g_closeKeySeen = false;
        g_awaitCloseKeyRelease = false;
    }
}

void QuitConfirmation::Confirm() {
    g_state = State::Closing;
    Hold(false);
    WindowManager::SetWindowShouldClose();
}

void QuitConfirmation::Cancel() {
    if (g_state != State::Prompting) return;
    g_state = State::Idle;

    if (g_prompt.Alive()) {
        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecs.HasComponent<ActiveComponent>(g_prompt.entity)) {
            ecs.GetComponent<ActiveComponent>(g_prompt.entity).isActive = false;
        }
        g_dismissed = g_prompt;
    }
    ForgetPrompt();
    g_rebaselineInput = true;
    Hold(false);
}

bool QuitConfirmation::IsPrompting() {
    return g_state == State::Prompting;
}

Entity QuitConfirmation::GetPromptRoot() {
    return g_state == State::Prompting && g_prompt.Alive() ? g_prompt.entity : INVALID_ENTITY;
}

bool QuitConfirmation::AcceptsPointerPresses() {
    return g_state == State::Prompting && g_framesShown > 1;
}

void QuitConfirmation::BeginFrame() {
    if (g_dismissed.Alive()) {
        ECSRegistry::GetInstance().GetActiveECSManager().DestroyEntity(g_dismissed.entity);
    }
    g_dismissed = Handle{};

    if (g_state != State::Prompting) return;

    if (g_awaitCloseKeyRelease && !IsCloseKeyHeld()) {
        g_awaitCloseKeyRelease = false;
    }

    if (!g_prompt.Alive()) {
        ForgetPrompt();
        const Entity prompt = InstantiatePrefabFromFile(kPromptPrefab);
        if (prompt == INVALID_ENTITY) {
            // Never leave the player unable to quit because the prompt could
            // not be shown.
            ENGINE_LOG_ERROR("[QuitConfirmation] Could not show the quit prompt; closing");
            Confirm();
            return;
        }
        // The prefab comes from the main menu, where the prompt waits hidden
        // until it is needed, so its root is saved inactive.
        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecs.HasComponent<ActiveComponent>(prompt)) {
            ecs.GetComponent<ActiveComponent>(prompt).isActive = true;
        }
        g_prompt = Handle::Of(prompt);
    }
    ++g_framesShown;
}

bool QuitConfirmation::ConsumeInputRebaseline() {
    const bool rebaseline = g_rebaselineInput;
    g_rebaselineInput = false;
    return rebaseline;
}

void QuitConfirmation::Reset() {
    // A prompt still in the scene is taken down the way No takes it down.
    if (g_prompt.Alive()) {
        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecs.HasComponent<ActiveComponent>(g_prompt.entity)) {
            ecs.GetComponent<ActiveComponent>(g_prompt.entity).isActive = false;
        }
        g_dismissed = g_prompt;
    }
    g_state = State::Idle;
    ForgetPrompt();
    g_lastWindowRequest.reset();
    g_closeKeySeen = false;
    g_awaitCloseKeyRelease = false;
    g_rebaselineInput = false;
    Hold(false);
}
