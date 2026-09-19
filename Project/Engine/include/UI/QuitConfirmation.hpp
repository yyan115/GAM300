#pragma once
#include "Engine.h"
#include "ECS/Entity.hpp"

// Asks before the game closes.
//
// A close request from the window, such as Alt+F4 or the title bar's close
// button, or from a script, such as the main menu's Exit button, shows one
// confirmation prompt. The prompt is a prefab, instantiated into whichever
// scene is running. While it is up the engine updates only the prompt: the rest
// of the scene, scene loads, game audio and scaled time stay where they were,
// and the cursor is shown. Yes, or a second close request from the window,
// closes the game. No removes the prompt and the game carries on.
//
// A held Alt+F4 repeats the request, and a repeat is not an answer to the
// prompt it opened. Where the game sees F4, a second request counts once F4
// has been let go; where it does not, once the requests have paused.
class ENGINE_API QuitConfirmation {
public:
    // The window was asked to close. Closes at once when the window is not
    // focused, since an unfocused game is suspended and could never show the
    // prompt.
    static void OnWindowCloseRequest();

    // Show the prompt, for a quit asked for from inside the game.
    static void Request();

    // Yes: close the game. Releases the hold first, since a process can be
    // reused after it closes, as an Android app is.
    static void Confirm();

    // No: remove the prompt and resume.
    static void Cancel();

    static bool IsPrompting();

    // The prompt's root entity while it is up, otherwise INVALID_ENTITY.
    static Entity GetPromptRoot();

    // Whether the prompt's buttons may take a press this frame. Not on the
    // frame it appears, so a press that arrived with it cannot answer it.
    static bool AcceptsPointerPresses();

    // Once a frame, after input is read and before the scene updates. Shows
    // the prompt if it should be up and is not, and removes one that was
    // dismissed.
    static void BeginFrame();

    // True once after the prompt is dismissed, when input should drop what it
    // accumulated while the cursor was shown.
    static bool ConsumeInputRebaseline();

    // Back to no prompt, with nothing held: when play stops in the editor, and
    // when the engine shuts down.
    static void Reset();
};
