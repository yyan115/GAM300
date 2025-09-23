#pragma once

// #ifdef ENGINE_EXPORTS
// #define ENGINE_API __declspec(dllexport)
// #else
// #define ENGINE_API __declspec(dllimport)
// #endif

// Cross-platform API export/import macros
#ifdef _WIN32
    #ifdef ENGINE_EXPORTS
        #define ENGINE_API __declspec(dllexport)
    #else
        #define ENGINE_API __declspec(dllimport)
    #endif
#else
    // Linux/GCC
    #ifdef ENGINE_EXPORTS
        #define ENGINE_API __attribute__((visibility("default")))
    #else
        #define ENGINE_API
    #endif
#endif

struct GLFWwindow;

enum class GameState {
    EDIT_MODE,
    PLAY_MODE,
    PAUSED_MODE
};

class ENGINE_API Engine {
public:
    static bool Initialize();
    static bool InitializeAssets(); // Android-specific delayed asset loading
    static bool InitializeGraphicsResources();
    static void Update();

    // Rendering phases
    static void StartDraw();
    static void Draw();
    static void EndDraw();

    static bool IsRunning();
    static void Shutdown();

    // Game state management
    static void SetGameState(GameState state);
    static GameState GetGameState();
    static bool ShouldRunGameLogic();
    static bool IsEditMode();
    static bool IsPlayMode();
    static bool IsPaused();

private:
    static GameState currentGameState;
};