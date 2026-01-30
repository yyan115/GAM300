#include "pch.h"
#include <ECS/ECSRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include <Scene/SceneInstance.hpp>
#include <filesystem>
#include <fstream>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <ECS/NameComponent.hpp>
#include "rapidjson/prettywriter.h"
#include <Serialization/Serializer.hpp>
#include "Utilities/GUID.hpp"
#include "Engine.h"
#include "Sound/AudioManager.hpp"

#ifdef _WIN32
	#include <windows.h>
	#include <shobjidl.h>   // IFileSaveDialog
	//#include <commdlg.h>    // OPENFILENAME
	#pragma comment(lib, "ole32.lib")
	#pragma comment(lib, "shell32.lib")
#endif
#include <Asset Manager/AssetManager.hpp>

SceneManager::~SceneManager() {
	//ExitScene();
}

SceneManager& SceneManager::GetInstance() {
    static SceneManager instance;
    return instance;
}

// Temporary function to load the test scene.
void SceneManager::LoadTestScene() {
	ECSRegistry::GetInstance().CreateECSManager("Resources/Scenes/FakeScene.scene");
	currentScene = std::make_unique<SceneInstance>("Resources/Scenes/FakeScene.scene");
	currentScenePath = "Resources/Scenes/FakeScene.scene";
	currentSceneName = "FakeScene";
	currentScene->Initialize();
}

// Load a new scene from the specified path.
// The current scene is exited and cleaned up before loading the new scene.
// Also sets the new scene as the active ECSManager in the ECSRegistry.
void SceneManager::LoadScene(const std::string& scenePath, bool fromGameCode) {
    // Check if we need to defer the load (scene not synchronized or called from Lua/script)
    // Skip deferral check if we're already executing a deferred load
    bool isDeferredExecution = isExecutingDeferredLoad;

    if (!isDeferredExecution && currentScene &&
        (fromGameCode || !currentScene->updateSynchronized || !currentScene->drawSynchronized)) {
		ENGINE_PRINT("[SceneManager] Deferring scene load to next frame: " + scenePath);
		// If the update/draw calls are not synchronized yet, defer loading to the next frame so that the scheduler can finish its work.
		// If calling from Lua/game code, defer loading to the next frame to allow the function call to be returned properly
		// before shutting down the scripting system in currentScene->Exit().
        loadSceneNextFrame = true;
        sceneToLoadNextFrame = scenePath;
        deferredSceneFromLua = fromGameCode;  // Remember if this was from game code
        return;
    }

#if 1
#ifdef EDITOR
	// Only reset to edit mode if loading scene from editor UI (not from game code)
	// When game code transitions scenes (e.g., main menu to game), we should stay in play mode
	if (!fromGameCode && (Engine::IsPlayMode() || Engine::IsPaused())) {
		// This is a manual scene load from editor while playing - exit play mode
		Engine::SetGameState(GameState::EDIT_MODE);
	}

#endif
	// Stop all audio when loading a new scene
	AudioManager::GetInstance().StopAll();

	// Exit and clean up the current scene if it exists.
	if (currentScene)
    {
		currentScene->Exit();

		ECSRegistry::GetInstance().GetECSManager(currentScenePath).ClearAllEntities();
		ECSRegistry::GetInstance().RenameECSManager(currentScenePath, scenePath);
	}
	else {
		ECSRegistry::GetInstance().CreateECSManager(scenePath);
	}

	// Create and initialize the new scene.
	currentScene = std::make_unique<SceneInstance>(scenePath);
	currentScenePath = scenePath;
	std::filesystem::path p(currentScenePath);
	currentSceneName = p.stem().generic_string();

    // MUST MAKE SURE JOLTPHYSICS IS INITIALIZED FIRST.
    currentScene->InitializeJoltPhysics();

	// Deserialize the new scene data.
	Serializer::DeserializeScene(scenePath);
    
	// Initialize the new scene.
	currentScene->Initialize();

	// Save this as the last opened scene (for editor persistence)
#ifdef EDITOR
	SaveLastOpenedScenePath(scenePath);
#endif
#else
#pragma region NEW
    // OPENS FILE DIALOG TO OPEN A SPECIFIC SCENE, TO BE IMPLEMENTED M2.
    namespace fs = std::filesystem;
    std::string chosenPath;

#if defined(_WIN32)
    bool gotPath = false;
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool coInitialized = SUCCEEDED(hrCo);
    if (!coInitialized)
    {
        ENGINE_LOG_WARN(std::string("[LoadScene] CoInitializeEx failed: ") + std::to_string(static_cast<long long>(hrCo)));
    }

    if (coInitialized)
    {
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
        if (SUCCEEDED(hr) && pFileOpen)
        {
            const COMDLG_FILTERSPEC fileTypes[] =
            {
                { L"JSON Files (*.json)", L"*.json" },
                { L"All Files (*.*)",     L"*.*"   }
            };

            pFileOpen->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
            pFileOpen->SetDefaultExtension(L"json");

            // Require file/path to exist
            DWORD options = 0;
            if (SUCCEEDED(pFileOpen->GetOptions(&options)))
            {
                pFileOpen->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
            }

            hr = pFileOpen->Show(nullptr); // pass owner HWND if available
            if (SUCCEEDED(hr))
            {
                IShellItem* pItem = nullptr;
                if (SUCCEEDED(pFileOpen->GetResult(&pItem)) && pItem)
                {
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath)
                    {
                        fs::path p(pszFilePath);
                        // use UTF-8 safe conversion
                        chosenPath = p.string();
                        CoTaskMemFree(pszFilePath);
                        gotPath = true;
                    }
                    pItem->Release();
                }
            }
            else
            {
                // user canceled
                if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
                {
                    pFileOpen->Release();
                    if (coInitialized) CoUninitialize();
                    return;
                }
                else
                {
                    ENGINE_LOG_WARN(std::string("[LoadScene] IFileOpenDialog::Show failed (hr): ") + std::to_string(static_cast<long long>(hr)));
                }
            }
            pFileOpen->Release();
        }
        else
        {
            ENGINE_LOG_WARN(std::string("[LoadScene] CoCreateInstance(CLSID_FileOpenDialog) failed (hr): ") + std::to_string(static_cast<long long>(hr)));
        }

        if (coInitialized) CoUninitialize();
    }
#else
    // Mobile / other: safe fallback � do not attempt desktop dialogs
    chosenPath = "scene.json";
    ENGINE_LOG_INFO(std::string("[LoadScene] Non-Windows platform; using fallback filename: ") + chosenPath);
#endif

    if (chosenPath.empty())
    {
        ENGINE_LOG_WARN("[LoadScene] No file selected; aborting load.");
        return;
    }

    // Verify file exists before proceeding
    try
    {
        if (!fs::exists(fs::path(chosenPath)))
        {
            ENGINE_LOG_WARN(std::string("[LoadScene] Selected file does not exist: ") + chosenPath);
            return;
        }
    }
    catch (const std::exception& ex)
    {
        ENGINE_LOG_WARN(std::string("[LoadScene] exception while checking file existence: ") + ex.what());
        return;
    }
    catch (...)
    {
        ENGINE_LOG_WARN("[LoadScene] unknown exception while checking file existence");
        return;
    }

    try
    {
        // Exit and clean up the current scene if it exists.
        if (currentScene)
        {
            currentScene->Exit();

            // Clear entities for the old manager and rename to the new path
            ECSRegistry::GetInstance().GetECSManager(currentScenePath).ClearAllEntities();
            ECSRegistry::GetInstance().RenameECSManager(currentScenePath, chosenPath);
        }
        else
        {
            // No existing manager: create manager for the new scene path
            ECSRegistry::GetInstance().CreateECSManager(chosenPath);
        }

        // Create and initialize the new scene instance
        currentScene = std::make_unique<SceneInstance>(chosenPath);
        currentScenePath = chosenPath;
        std::filesystem::path p(currentScenePath);
        currentSceneName = p.stem().generic_string();

        // Deserialize the new scene data.
        Serializer::DeserializeScene(chosenPath);

        // Initialize the new scene instance.
        currentScene->Initialize();

        ENGINE_LOG_INFO(std::string("[LoadScene] successfully loaded scene: ") + chosenPath);
    }
    catch (const std::exception& ex)
    {
        ENGINE_LOG_WARN(std::string("[LoadScene] exception while loading scene: ") + ex.what());
    }
    catch (...)
    {
        ENGINE_LOG_WARN("[LoadScene] unknown exception while loading scene");
    }
#pragma endregion
#endif
}

void SceneManager::UpdateScene(double dt) {
    if (loadSceneNextFrame && currentScene->updateSynchronized && currentScene->drawSynchronized) {
		ENGINE_PRINT("[SceneManager] Loading deferred scene this frame: " + sceneToLoadNextFrame);
        std::string pathToLoad = sceneToLoadNextFrame;
        bool wasFromGameCode = deferredSceneFromLua;  // Save before clearing

        // Clear deferred state
        loadSceneNextFrame = false;
        sceneToLoadNextFrame = "";
        deferredSceneFromLua = false;

        // Set flag to prevent re-deferral during this execution
        isExecutingDeferredLoad = true;
        LoadScene(pathToLoad, wasFromGameCode);
        isExecutingDeferredLoad = false;
    }
	if (currentScene) {
		currentScene->Update(dt);
	}
}

void SceneManager::DrawScene() {
	if (currentScene) {
		currentScene->Draw();
		//std::cout << "drawn scene scenemanager\n";
	}
}

void SceneManager::ExitScene() {
	if (currentScene) {
		//Serializer::GetInstance().SerializeScene(currentScenePath);
		currentScene->Exit();
		currentScene.reset();
		currentScenePath.clear();
	}
}

void SceneManager::SaveScene() 
{
    Serializer::SerializeScene(currentScenePath);

    // Save to BOTH the Editor/Resources/Scenes folder and the ROOT PROJECT Resources/Scenes folder to ensure ALL scene files are synced when saved.
    std::filesystem::path editorScenesPath(currentScenePath.substr(currentScenePath.find("Resources")));
    if (currentScenePath != editorScenesPath.generic_string()) {
        if (FileUtilities::StrictExists(editorScenesPath)) {
            if (FileUtilities::CopyFile(currentScenePath, editorScenesPath.generic_string())) {
                ENGINE_LOG_INFO("[SceneManager] Scene saved to Editor/Resources/Scenes: " + editorScenesPath.generic_string());
            }
            else {
                ENGINE_LOG_WARN("[SceneManager] Failed to copy scene to Editor/Resources/Scenes: " + editorScenesPath.generic_string());
            }
        }
        else {
            ENGINE_LOG_WARN("[SceneManager] Editor/Resources/Scenes path does not exist: " + editorScenesPath.generic_string());
        }
    }
    else {
		ENGINE_LOG_DEBUG("[SceneManager] Current scene path is already in Editor/Resources/Scenes: " + currentScenePath);
    }

	std::filesystem::path projectRootScenesPath(std::filesystem::path(AssetManager::GetInstance().GetRootAssetDirectory()) / std::filesystem::path(currentScenePath.substr(currentScenePath.find("Scenes"))));
    if (currentScenePath != projectRootScenesPath.generic_string()) {
        if (FileUtilities::StrictExists(projectRootScenesPath)) {
            if (FileUtilities::CopyFile(currentScenePath, projectRootScenesPath.generic_string())) {
                ENGINE_LOG_INFO("[SceneManager] Scene saved to Root Project/Resources/Scenes: " + projectRootScenesPath.generic_string());
            }
            else {
                ENGINE_LOG_WARN("[SceneManager] Failed to copy scene to Root Project/Resources/Scenes: " + projectRootScenesPath.generic_string());
            }
	    }
        else {
		    ENGINE_LOG_WARN("[SceneManager] Root Project/Resources/Scenes path does not exist: " + projectRootScenesPath.generic_string());
        }
    }
    else {
		ENGINE_LOG_DEBUG("[SceneManager] Current scene path is already in Root Project/Resources/Scenes: " + currentScenePath);
    }

// COMMENTED PART BELOW OPENS A FILE DIALOG WINDOW TO SAVE THE SCENE TO A SPECIFIC LOCATION, TO BE IMPLEMENTED M2.
//    namespace fs = std::filesystem;
//    std::string targetPath;
//
//#if defined(_WIN32)
//
//    // Try modern Vista+ dialog first (IFileSaveDialog)
//    bool gotPath = false;
//    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
//    bool coInitialized = SUCCEEDED(hrCo);
//
//    if (coInitialized)
//    {
//        IFileSaveDialog* pFileSave = nullptr;
//        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileSave));
//        if (SUCCEEDED(hr) && pFileSave)
//        {
//            const COMDLG_FILTERSPEC fileTypes[] =
//            {
//                { L"Scene Files (*.scene)", L"*.scene" },
//                { L"All Files (*.*)",     L"*.*"   }
//            };
//
//            pFileSave->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
//            pFileSave->SetDefaultExtension(L"scene");
//            pFileSave->SetFileName(L"New Scene.scene"); // suggested filename
//
//            hr = pFileSave->Show(nullptr); // pass HWND if available
//            if (SUCCEEDED(hr))
//            {
//                IShellItem* pItem = nullptr;
//                if (SUCCEEDED(pFileSave->GetResult(&pItem)) && pItem)
//                {
//                    PWSTR pszFilePath = nullptr;
//                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath)
//                    {
//                        fs::path chosen(pszFilePath);
//                        targetPath = chosen.string();
//                        CoTaskMemFree(pszFilePath);
//                        gotPath = true;
//                    }
//                    pItem->Release();
//                }
//            }
//            else
//            {
//                // If user cancelled, HRESULT == HRESULT_FROM_WIN32(ERROR_CANCELLED)
//                if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
//                {
//                    // User cancelled the save dialog � do nothing.
//                    pFileSave->Release();
//                    if (coInitialized) CoUninitialize();
//                    return;
//                }
//            }
//
//            pFileSave->Release();
//        }
//        // Uninitialize COM for this call (matching CoInitializeEx)
//        if (coInitialized) CoUninitialize();
//    }
//
//    // If modern dialog didn't succeed (either COM failed or dialog not available), fallback to legacy API
//    #pragma region MISSING WINDOWS LIB
//        //if (!gotPath)
//        //{
//        //    wchar_t szFile[MAX_PATH] = L"scene.json";
//        //    OPENFILENAMEW ofn = {};
//        //    ofn.lStructSize = sizeof(ofn);
//        //    ofn.hwndOwner = nullptr; // replace with your HWND if available
//        //    ofn.lpstrFile = szFile;
//        //    ofn.nMaxFile = ARRAYSIZE(szFile);
//        //    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0\0";
//        //    ofn.lpstrTitle = L"Save scene as...";
//        //    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
//
//        //    if (GetSaveFileNameW(&ofn))
//        //    {
//        //        fs::path chosen(szFile);
//        //        targetPath = chosen.string();
//        //        gotPath = true;
//        //    }
//        //    else
//        //    {
//        //        DWORD err = CommDlgExtendedError();
//        //        if (err != 0)
//        //        {
//        //            ENGINE_LOG_WARN("[SaveScene] GetSaveFileNameW failed, error = " + err);
//        //        }
//        //        else
//        //        {
//        //            // user cancelled the legacy dialog as well -> abort
//        //            return;
//        //        }
//        //    }
//        //}
//    #pragma endregion
//
//#else
//    // Non-Windows (mobile / other): DO NOT call any OS desktop dialogs.
//    // Fallback to a safe default filename. Serializer has its own fallback behavior
//    // for directory creation � it will try to write to cwd if needed.
//    targetPath = "scene.json";
//    ENGINE_LOG_INFO("[SaveScene] Non-Windows platform; using fallback filename: " + targetPath);
//#endif
//
//    // Final guard: ensure we have a target path
//    if (targetPath.empty())
//    {
//        ENGINE_LOG_WARN("[SaveScene] No valid target path provided; aborting save.");
//        return;
//    }
//
//    // Call serializer. Wrap in try/catch to prevent exceptions bubbling up.
//    try
//    {
//        // If Serializer signature differs (bool return, etc.), adapt this call accordingly.
//        Serializer::SerializeScene(targetPath);
//        ENGINE_LOG_INFO("[SaveScene] serialize requested for: " + targetPath);
//    }
//    catch (const std::exception& ex)
//    {
//        ENGINE_LOG_WARN("[SaveScene] exception while saving scene: " + static_cast<std::string>(ex.what()));
//    }
//    catch (...)
//    {
//        ENGINE_LOG_WARN("[SaveScene] unknown exception while saving scene");
//    }
}

void SceneManager::InitializeScenePhysics() {
    currentScene->InitializePhysics();
}

void SceneManager::ShutDownScenePhysics() {
    currentScene->ShutDownPhysics();
}

void SceneManager::SaveTempScene() {
	// Serialize the current scene data to a temporary file.
	std::string tempScenePath = currentScenePath + ".temp";
	Serializer::SerializeScene(tempScenePath);
}

void SceneManager::ReloadTempScene() {
	std::string tempScenePath = currentScenePath + ".temp";
	if (std::filesystem::exists(tempScenePath)) {
		Serializer::ReloadScene(tempScenePath, currentScenePath);

        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

        // Re-initialise/reload required systems.
        ecs.cameraSystem->Initialise();
        ecs.modelSystem->Initialise();
        ecs.textSystem->Initialise();
        ecs.spriteAnimationSystem->Initialise();
        ecs.particleSystem->Initialise(true);
        ecs.animationSystem->Initialise();
        ecs.scriptSystem->ReloadSystem();

        // Reset all animation preview states to 0 (fresh editor state)
        for (auto ent : ecs.GetActiveEntities()) {
            if (ecs.HasComponent<AnimationComponent>(ent)) {
                AnimationComponent& animComp = ecs.GetComponent<AnimationComponent>(ent);
                animComp.ResetPreview(ent);
            }
        }
	}
	else {
		// Handle the case where the temp file doesn't exist (e.g., for newly created scenes)
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "Temp file does not exist, skipping reload: ", tempScenePath, "\n");
		return; // Early exit if needed
	}
}

std::string SceneManager::GetSceneName() const {
	return currentSceneName;
}

void SceneManager::UpdateScenePath(const std::string& oldPath, const std::string& newPath) {
	if (currentScenePath == oldPath) {
		currentScenePath = newPath;
		std::filesystem::path p(newPath);
		currentSceneName = p.stem().generic_string();

		SaveLastOpenedScenePath(newPath);

		ENGINE_LOG_INFO("[SceneManager] Updated scene path from " + oldPath + " to " + newPath);
	}
}

void SceneManager::SaveLastOpenedScenePath(const std::string& scenePath) {
	namespace fs = std::filesystem;
	try {
		// Use project root directory (parent of current working directory which is usually Build/EditorRelease)
		fs::path projectRoot = fs::current_path().parent_path().parent_path();
		fs::path settingsDir = projectRoot / "ProjectSettings";

		// Create ProjectSettings directory if it doesn't exist
		if (!fs::exists(settingsDir)) {
			fs::create_directories(settingsDir);
		}

		fs::path lastSceneFile = settingsDir / "last_scene.txt";
		std::ofstream file(lastSceneFile);
		if (file.is_open()) {
			file << scenePath;
			file.close();
			ENGINE_LOG_INFO("[SceneManager] Saved last opened scene path to: " + lastSceneFile.string());
		}
		else {
			ENGINE_LOG_WARN("[SceneManager] Failed to save last opened scene path");
		}
	}
	catch (const std::exception& ex) {
		ENGINE_LOG_WARN(std::string("[SceneManager] Exception saving last scene path: ") + ex.what());
	}
}

std::string SceneManager::LoadLastOpenedScenePath() {
	namespace fs = std::filesystem;
	try {
		// Use project root directory (parent of current working directory which is usually Build/EditorRelease)
		fs::path projectRoot = fs::current_path().parent_path().parent_path();
		fs::path settingsDir = projectRoot / "ProjectSettings";
		fs::path lastSceneFile = settingsDir / "last_scene.txt";

		std::ifstream file(lastSceneFile);
		if (file.is_open()) {
			std::string scenePath;
			std::getline(file, scenePath);
			file.close();
			if (!scenePath.empty() && fs::exists(scenePath)) {
				ENGINE_LOG_INFO("[SceneManager] Loaded last opened scene path: " + scenePath);
				return scenePath;
			}
			else if (!scenePath.empty()) {
				ENGINE_LOG_WARN("[SceneManager] Last opened scene no longer exists: " + scenePath);
			}
		}
		else {
			ENGINE_LOG_INFO("[SceneManager] No last scene file found at: " + lastSceneFile.string());
		}
	}
	catch (const std::exception& ex) {
		ENGINE_LOG_WARN(std::string("[SceneManager] Exception loading last scene path: ") + ex.what());
	}

	// Return default scene if no saved path or file doesn't exist
	return "";
}

void SceneManager::CreateNewScene(const std::string& directory, bool loadAfterCreate) {
    static int sceneCounter = 0;
    std::string newSceneName = "New Scene.scene";
    std::filesystem::path directoryPath(directory);
    std::filesystem::path newSceneNamePath(newSceneName);
    std::string stem = newSceneNamePath.stem().generic_string();
    std::string extension = newSceneNamePath.extension().generic_string();

    // Generate unique name, checking for existing files (similar to AssetBrowserPanel pattern)
    std::string uniqueName;
    std::filesystem::path newScenePathFull;
    do {
        std::string suffix = (sceneCounter > 0) ? "_" + std::to_string(sceneCounter) : "";
        uniqueName = stem + suffix + extension;
        newScenePathFull = directoryPath / uniqueName;
        sceneCounter++;
    } while (std::filesystem::exists(newScenePathFull));

    std::string scenePath = newScenePathFull.generic_string();

    // Ensure the directory exists
    std::filesystem::path parentDir = newScenePathFull.parent_path();
    std::filesystem::create_directories(parentDir);

    // Generate GUID for the camera entity
    std::string guidStr = GUIDUtilities::GenerateGUIDString();

    // Write a minimal scene JSON with a default Main Camera entity
    std::ofstream file(scenePath);
    if (file.is_open()) {
        // Write minimal scene JSON with Main Camera
        file << R"({
    "entities": [
        {
            "id": 0,
            "guid": ")" << guidStr << R"(",
            "components": {
                "NameComponent": {
                    "name": "Main Camera"
                },
                "TagComponent": {
                    "tagIndex": 0
                },
                "LayerComponent": {
                    "layerIndex": 0
                },
                "Transform": {
                    "type": "Transform",
                    "data": [
                        { "type": "Vector3D", "data": [
                            { "type": "float", "data": 0 },
                            { "type": "float", "data": 0 },
                            { "type": "float", "data": 5 }
                        ]},
                        { "type": "Vector3D", "data": [
                            { "type": "float", "data": 1 },
                            { "type": "float", "data": 1 },
                            { "type": "float", "data": 1 }
                        ]},
                        { "type": "Quaternion", "data": [
                            { "type": "float", "data": 1 },
                            { "type": "float", "data": 0 },
                            { "type": "float", "data": 0 },
                            { "type": "float", "data": 0 }
                        ]},
                        { "type": "bool", "data": false },
                        { "type": "Matrix4x4", "data": [] }
                    ]
                },
                "CameraComponent": {
                    "type": "CameraComponent",
                    "data": [
                        { "type": "bool", "data": true },
                        { "type": "bool", "data": true },
                        { "type": "int", "data": 0 },
                        { "type": "float", "data": -90 },
                        { "type": "float", "data": 0 },
                        { "type": "bool", "data": true },
                        { "type": "float", "data": 45 },
                        { "type": "float", "data": 0.1 },
                        { "type": "float", "data": 1000 },
                        { "type": "float", "data": 5 },
                        { "type": "float", "data": 2.5 },
                        { "type": "float", "data": 0.1 },
                        { "type": "float", "data": 1 },
                        { "type": "float", "data": 90 },
                        { "type": "float", "data": 0 },
                        { "type": "float", "data": 0 },
                        { "type": "float", "data": 0 },
                        { "type": "float", "data": 0 },
                        "0000000000000000-0000000000000000"
                    ]
                },
                "ActiveComponent": {
                    "type": "ActiveComponent",
                    "data": [
                        { "type": "bool", "data": true }
                    ]
                }
            }
        }
    ]
})";
        file.close();
        ENGINE_LOG_INFO("[SceneManager] Created new scene with default camera: " + scenePath);
        if (loadAfterCreate) {
            LoadScene(scenePath);
        }
    }
    else {
        ENGINE_LOG_ERROR("[SceneManager] Failed to create scene file: " + scenePath);
    }
}
