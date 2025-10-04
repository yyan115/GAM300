#include "pch.h"
#include <ECS/ECSRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include <Scene/SceneInstance.hpp>
#include <filesystem>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <ECS/NameComponent.hpp>
#include "rapidjson/prettywriter.h"
#include <Serialization/Serializer.hpp>
#include "Logging.hpp"
#include <Utilities/FileUtilities.hpp>

#ifdef _WIN32
	#include <windows.h>
	#include <shobjidl.h>   // IFileSaveDialog
	//#include <commdlg.h>    // OPENFILENAME
	#pragma comment(lib, "ole32.lib")
	#pragma comment(lib, "shell32.lib")
#endif

SceneManager::~SceneManager() {
	//ExitScene();
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
void SceneManager::LoadScene(const std::string& scenePath) {
#if 1
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

	// Deserialize the new scene data.
	Serializer::DeserializeScene(scenePath);
    
	// Initialize the new scene.
	currentScene->Initialize();
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
    // Mobile / other: safe fallback — do not attempt desktop dialogs
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
//                    // User cancelled the save dialog — do nothing.
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
//    // for directory creation — it will try to write to cwd if needed.
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

void SceneManager::SaveTempScene() {
	// Serialize the current scene data to a temporary file.
	std::string tempScenePath = currentScenePath + ".temp";
	//Serializer::GetInstance().SerializeScene(tempScenePath);
}

void SceneManager::ReloadTempScene() {
	std::string tempScenePath = currentScenePath + ".temp";
	if (std::filesystem::exists(tempScenePath)) {
		//Serializer::GetInstance().ReloadScene(tempScenePath);
	}
	else {
		// Handle the case where the temp file doesn't exist
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "Temp file does not exist: ", tempScenePath, "\n");
		return; // Early exit if needed
	}
}

std::string SceneManager::GetSceneName() const {
	return currentSceneName;
}
