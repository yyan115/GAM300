#include "pch.h"

#include "Graphics/OpenGL.h"
#include "Platform/Platform.h"
#include "Graphics/LightManager.hpp"

#include "Engine.h"
#include "Logging.hpp"

#include <WindowManager.hpp>
#include <Input/InputManager.hpp>
#include <Asset Manager/MetaFilesManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include "TimeManager.hpp"
#include <Sound/AudioManager.hpp>

namespace TEMP {
	std::string windowTitle = "GAM300";
}

// Static member definition
GameState Engine::currentGameState = GameState::EDIT_MODE;

const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

//RenderSystem& renderer = RenderSystem::getInstance();
//std::shared_ptr<Model> backpackModel;
//std::shared_ptr<Shader> shader;
////----------------LIGHT-------------------
//std::shared_ptr<Shader> lightShader;
//std::shared_ptr<Mesh> lightCubeMesh;

bool Engine::Initialize() {
	// Initialize logging system first
	if (!EngineLogging::Initialize()) {
		std::cerr << "[Engine] Failed to initialize logging system!" << std::endl;
		return false;
	}
	SetGameState(GameState::PLAY_MODE);
	WindowManager::Initialize(SCR_WIDTH, SCR_HEIGHT, TEMP::windowTitle.c_str());

    ENGINE_PRINT("Engine initializing...");
    //ENGINE_LOG_INFO("Engine initializing...");

	// WOON LI TEST CODE
	InputManager::Initialize();
	MetaFilesManager::InitializeAssetMetaFiles("Resources");

	//TEST ON ANDROID FOR REFLECTION - IF NOT WORKING, INFORM IMMEDIATELY
#if 1
	{
        bool reflection_ok = true;
        bool serialization_ok = true;

        ENGINE_PRINT("=== Running reflection + serialization single-main test for Vector3D ===\n");

        // --- Reflection-only checks ---
        ENGINE_PRINT("[1] Reflection metadata + runtime access checks\n");

        using T = Vector3D;
        TypeDescriptor* td = nullptr;
        try {
            td = TypeResolver<T>::Get();
        }
        catch (const std::exception& ex) {
            //std::cout << "ERROR: exception while calling TypeResolver::Get(): " << ex.what() << "\n";
            ENGINE_PRINT(
                std::string("ERROR: exception while calling TypeResolver::Get(): ") + ex.what(),
                EngineLogging::LogLevel::Error);
        }
        catch (...) {
            ENGINE_PRINT("ERROR: unknown exception calling TypeResolver::Get()\n", EngineLogging::LogLevel::Error);
            //std::cout << "ERROR: unknown exception calling TypeResolver::Get()\n";
        }

        if (!td) {
            //std::cout << "FAIL: TypeResolver<Vector3D>::Get() returned null. Ensure REFL_REGISTER_START(Vector3D) is compiled & linked.\n";
            ENGINE_PRINT(
                "FAIL: TypeResolver<Vector3D>::Get() returned null. Ensure REFL_REGISTER_START(Vector3D) is compiled & linked.\n"
                , EngineLogging::LogLevel::Error);

            reflection_ok = false;
        }
        else {
            //std::cout << "Type name: " << td->ToString() << ", size: " << td->size << "\n";
            ENGINE_PRINT(
                std::string("Type name: ") + td->ToString() + ", size: " + std::to_string(td->size),
                EngineLogging::LogLevel::Debug
            );
            auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
            if (!sdesc) {
                ENGINE_PRINT("FAIL: descriptor is not TypeDescriptor_Struct\n", EngineLogging::LogLevel::Error);
                //std::cout << "FAIL: descriptor is not TypeDescriptor_Struct\n";
                reflection_ok = false;
            }
            else {
                ENGINE_PRINT(std::string("Member count: ") + std::to_string(sdesc->members.size()) + std::string("\n"), EngineLogging::LogLevel::Debug);
                //std::cout << "Member count: " << sdesc->members.size() << "\n";
                // Print members and basic checks
                for (size_t i = 0; i < sdesc->members.size(); ++i) {
                    const auto& m = sdesc->members[i];
                    std::string mname = m.name ? m.name : "<null>";
                    std::string tname = m.type ? m.type->ToString() : "<null-type>";
                    //std::cout << "  [" << i << "] name='" << mname << "' type='" << tname << "'\n";
                    ENGINE_PRINT(
                        std::string("  [") + std::to_string(i) + "] name='" + mname + "' type='" + tname + "'",
                        EngineLogging::LogLevel::Debug
                    );
                    if (!m.type) {
                        ENGINE_PRINT(
                            "    -> FAIL: member has null TypeDescriptor\n",
                            EngineLogging::LogLevel::Error
                        );
                        reflection_ok = false;
                    }
                    if (tname.find('&') != std::string::npos) {
                        ENGINE_PRINT(
                            "    -> FAIL: member type contains '&' (strip references in macro). See REFL_REGISTER_PROPERTY fix.\n",
                            EngineLogging::LogLevel::Error
                        );
                        reflection_ok = false;
                    }
                    if (!m.get_ptr) {
                        ENGINE_PRINT(
                            "    -> FAIL: member.get_ptr is null\n",
                            EngineLogging::LogLevel::Error
                        );
                        reflection_ok = false;
                    }
                }

                // Runtime read/write via get_ptr to prove reflection can access object memory
                try {
                    T v{};
                    if (sdesc->members.size() >= 1) *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&v)) = 1.2345f;
                    if (sdesc->members.size() >= 2) *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&v)) = 2.5f;
                    if (sdesc->members.size() >= 3) *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&v)) = -7.125f;

                    bool values_ok = true;
                    if (sdesc->members.size() >= 3) {
                        float a = *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&v));
                        float b = *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&v));
                        float c = *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&v));
                        if (!(a == 1.2345f && b == 2.5f && c == -7.125f)) values_ok = false;
                    }
                    else {
                        ENGINE_PRINT(
                            "    -> WARN: fewer than 3 members; cannot fully validate values\n",
                            EngineLogging::LogLevel::Warn
                        );
                        values_ok = false;
                    }
                    ENGINE_PRINT(
                        std::string("  Runtime read/write via get_ptr: ") + (values_ok ? "OK" : "MISMATCH") + "\n",
                        EngineLogging::LogLevel::Info
                    );
                    if (!values_ok) reflection_ok = false;
                }
                catch (const std::exception& ex) {
                    ENGINE_PRINT(
                        std::string("    -> FAIL: exception during runtime read/write: ") + ex.what() + "\n",
                        EngineLogging::LogLevel::Error
                    );
                    reflection_ok = false;
                }
                catch (...) {
                    ENGINE_PRINT(
                        "    -> FAIL: unknown exception during runtime read/write\n",
                        EngineLogging::LogLevel::Error
                    );
                    reflection_ok = false;
                }
            }
        }

        // --- Serialization checks (uses TypeDescriptor::Serialize / SerializeJson / Deserialize) ---
        ENGINE_PRINT(
            "\n[2] Serialization + round-trip checks\n",
            EngineLogging::LogLevel::Info
        );
        if (!td) {
            ENGINE_PRINT(
                "SKIP: serialization checks because TypeDescriptor was not available\n",
                EngineLogging::LogLevel::Warn
            );
            serialization_ok = false;
        }
        else {
            try {
                // Create sample object and populate via reflection
                T src{};
                auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
                if (!sdesc) {
                    ENGINE_PRINT(
                        "FAIL: not a struct descriptor; cannot serialize\n",
                        EngineLogging::LogLevel::Error
                    );
                    serialization_ok = false;
                }
                else {
                    if (sdesc->members.size() >= 3) {
                        *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&src)) = 10.0f;
                        *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&src)) = -3.5f;
                        *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&src)) = 0.25f;
                    }
                    else {
                        ENGINE_PRINT(
                            "  WARN: not enough members to populate canonical values\n",
                            EngineLogging::LogLevel::Warn
                        );
                    }
                    // 1) Text Serialize
                    std::stringstream ss;
                    td->Serialize(&src, ss);
                    std::string text_out = ss.str();
                    ENGINE_PRINT(
                        std::string("  Text Serialize output: ") + text_out + "\n",
                        EngineLogging::LogLevel::Debug
                    );
                    // 2) rapidjson SerializeJson -> string
                    rapidjson::Document dout;
                    td->SerializeJson(&src, dout);
                    rapidjson::StringBuffer sb;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
                    dout.Accept(writer);
                    ENGINE_PRINT(
                        std::string("  rapidjson Serialize output: ") + sb.GetString() + "\n",
                        EngineLogging::LogLevel::Debug
                    );
                    // 3) Round-trip deserialize
                    T dst{};
                    rapidjson::Document din;
                    din.Parse(sb.GetString());
                    td->Deserialize(&dst, din);

                    bool match = true;
                    if (sdesc->members.size() >= 3) {
                        float a = *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&src));
                        float b = *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&src));
                        float c = *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&src));
                        float a2 = *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&dst));
                        float b2 = *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&dst));
                        float c2 = *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&dst));
                        if (!(a == a2 && b == b2 && c == c2)) match = false;
                    }
                    else {
                        match = false;
                    }

                    ENGINE_PRINT(
                        std::string("  Round-trip equality: ") + (match ? "OK" : "MISMATCH") + "\n",
                        EngineLogging::LogLevel::Info
                    );
                    if (!match) serialization_ok = false;
                }
            }
            catch (const std::exception& ex) {
                ENGINE_PRINT(
                    std::string("FAIL: exception during serialization tests: ") + ex.what() + "\n",
                    EngineLogging::LogLevel::Error
                );
                serialization_ok = false;
            }
            catch (...) {
                ENGINE_PRINT(
                    "FAIL: unknown error during serialization tests\n",
                    EngineLogging::LogLevel::Error
                );
                serialization_ok = false;
            }
        }

        // --- Registry introspection (optional) ---
        ENGINE_PRINT(
            "\n[3] Registry contents (keys):\n",
            EngineLogging::LogLevel::Info
        );
        for (const auto& kv : TypeDescriptor::type_descriptor_lookup()) {
            ENGINE_PRINT(
                std::string("  ") + kv.first + "\n",
                EngineLogging::LogLevel::Debug
            );
        }
        // --- Summary & exit code ---

        ENGINE_PRINT("\n=== SUMMARY ===\n");
        ENGINE_PRINT(
            std::string("Reflection: ") + (reflection_ok ? "PASS" : "FAIL") + "\n",
            reflection_ok ? EngineLogging::LogLevel::Info : EngineLogging::LogLevel::Error
        );
        ENGINE_PRINT(
            std::string("Serialization: ") + (serialization_ok ? "PASS" : "FAIL") + "\n",
            serialization_ok ? EngineLogging::LogLevel::Info : EngineLogging::LogLevel::Error
        );

        if (!reflection_ok) {
            ENGINE_PRINT(
                R"(
NOTE: if you hit a linker error mentioning GetPrimitiveDescriptor<float&>() or you see member types printed with '&',
apply the macro fix to strip references when resolving member types in the macro:
Replace the TypeResolver line in REFL_REGISTER_PROPERTY with:
  TypeResolver<std::remove_reference_t<decltype(std::declval<T>().VARIABLE)>>::Get()
This prevents requesting descriptors for reference types (e.g. float&).
)" "\n",
EngineLogging::LogLevel::Warn
);
        }

	}
#endif

	// Load test scene
	SceneManager::GetInstance().LoadTestScene();

	// ---Set Up Lighting---
	LightManager& lightManager = LightManager::getInstance();
	const auto& pointLights = lightManager.getPointLights();
	// Set up directional light
	lightManager.setDirectionalLight(
		glm::vec3(-0.2f, -1.0f, -0.3f),
		glm::vec3(0.4f, 0.4f, 0.4f)
	);

	// Add point lights
	glm::vec3 lightPositions[] = {
		glm::vec3(0.7f,  0.2f,  2.0f),
		glm::vec3(2.3f, -3.3f, -4.0f),
		glm::vec3(-4.0f,  2.0f, -12.0f),
		glm::vec3(0.0f,  0.0f, -3.0f)
	};

	for (int i = 0; i < 4; i++) 
	{
		lightManager.addPointLight(lightPositions[i], glm::vec3(0.8f, 0.8f, 0.8f));
	}

	// Set up spotlight
	lightManager.setSpotLight(
		glm::vec3(0.0f),
		glm::vec3(0.0f, 0.0f, -1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f)
	);

	//lightManager.printLightStats();

	// Test Audio
	{
		if (!AudioManager::StaticInitalize())
		{
			ENGINE_PRINT("Failed to initialize AudioManager",EngineLogging::LogLevel::Error);
		}
		else
		{
			AudioManager::StaticLoadSound("test_sound", "Test_duck.wav", false);
			AudioManager::StaticPlaySound("test_sound", 0.5f, 1.0f);
		}
	}

    ENGINE_PRINT("Engine initialization completed successfully");
	
	// Add some test logging messages
    ENGINE_PRINT("This is a test warning message",EngineLogging::LogLevel::Warn);
    ENGINE_PRINT("This is a test error message",EngineLogging::LogLevel::Error);
	
	return true;
}

void Engine::Update() {
	// Only update the scene if the game should be running (not paused)
	if (ShouldRunGameLogic()) {
        SceneManager::GetInstance().UpdateScene(TimeManager::GetDeltaTime()); // REPLACE WITH DT LATER


		// Test Audio
		AudioManager::StaticUpdate();
	}
}

void Engine::StartDraw() {
}

void Engine::Draw() {
	SceneManager::GetInstance().DrawScene();
	
}

void Engine::EndDraw() {
	WindowManager::SwapBuffers();

	// Only process input if the game should be running (not paused)
	if (ShouldRunGameLogic()) {
		InputManager::Update();
	}

	WindowManager::PollEvents(); // Always poll events for UI and window management
}

void Engine::Shutdown() {
	ENGINE_LOG_INFO("Engine shutdown started");
	AudioManager::StaticShutdown();
    EngineLogging::Shutdown();
    ENGINE_PRINT("[Engine] Shutdown complete\n"); 
}

bool Engine::IsRunning() {
	return !WindowManager::ShouldClose();
}

// Game state management functions
void Engine::SetGameState(GameState state) {
	currentGameState = state;
}

GameState Engine::GetGameState() {
	return currentGameState;
}

bool Engine::ShouldRunGameLogic() {
	return currentGameState == GameState::PLAY_MODE;
}

bool Engine::IsEditMode() {
	return currentGameState == GameState::EDIT_MODE;
}

bool Engine::IsPlayMode() {
	return currentGameState == GameState::PLAY_MODE;
}

bool Engine::IsPaused() {
	return currentGameState == GameState::PAUSED_MODE;
}