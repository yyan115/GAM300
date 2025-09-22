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

    ENGINE_LOG_INFO("Engine initializing...");

	// WOON LI TEST CODE
	InputManager::Initialize();
	MetaFilesManager::InitializeAssetMetaFiles("Resources");

	//TEST ON ANDROID FOR REFLECTION - IF NOT WORKING, INFORM IMMEDIATELY
#if 1
	{
        bool reflection_ok = true;
        bool serialization_ok = true;

        std::cout << "=== Running reflection + serialization single-main test for Vector3D ===\n";

        // --- Reflection-only checks ---
        std::cout << "\n[1] Reflection metadata + runtime access checks\n";
        using T = Vector3D;
        TypeDescriptor* td = nullptr;
        try {
            td = TypeResolver<T>::Get();
        }
        catch (const std::exception& ex) {
            std::cout << "ERROR: exception while calling TypeResolver::Get(): " << ex.what() << "\n";
        }
        catch (...) {
            std::cout << "ERROR: unknown exception calling TypeResolver::Get()\n";
        }

        if (!td) {
            std::cout << "FAIL: TypeResolver<Vector3D>::Get() returned null. Ensure REFL_REGISTER_START(Vector3D) is compiled & linked.\n";
            reflection_ok = false;
        }
        else {
            std::cout << "Type name: " << td->ToString() << ", size: " << td->size << "\n";
            auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
            if (!sdesc) {
                std::cout << "FAIL: descriptor is not TypeDescriptor_Struct\n";
                reflection_ok = false;
            }
            else {
                std::cout << "Member count: " << sdesc->members.size() << "\n";
                // Print members and basic checks
                for (size_t i = 0; i < sdesc->members.size(); ++i) {
                    const auto& m = sdesc->members[i];
                    std::string mname = m.name ? m.name : "<null>";
                    std::string tname = m.type ? m.type->ToString() : "<null-type>";
                    std::cout << "  [" << i << "] name='" << mname << "' type='" << tname << "'\n";
                    if (!m.type) {
                        std::cout << "    -> FAIL: member has null TypeDescriptor\n";
                        reflection_ok = false;
                    }
                    if (tname.find('&') != std::string::npos) {
                        std::cout << "    -> FAIL: member type contains '&' (strip references in macro). See REFL_REGISTER_PROPERTY fix.\n";
                        reflection_ok = false;
                    }
                    if (!m.get_ptr) {
                        std::cout << "    -> FAIL: member.get_ptr is null\n";
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
                        std::cout << "    -> WARN: fewer than 3 members; cannot fully validate values\n";
                        values_ok = false;
                    }

                    std::cout << "  Runtime read/write via get_ptr: " << (values_ok ? "OK" : "MISMATCH") << "\n";
                    if (!values_ok) reflection_ok = false;
                }
                catch (const std::exception& ex) {
                    std::cout << "    -> FAIL: exception during runtime read/write: " << ex.what() << "\n";
                    reflection_ok = false;
                }
                catch (...) {
                    std::cout << "    -> FAIL: unknown exception during runtime read/write\n";
                    reflection_ok = false;
                }
            }
        }

        // --- Serialization checks (uses TypeDescriptor::Serialize / SerializeJson / Deserialize) ---
        std::cout << "\n[2] Serialization + round-trip checks\n";
        if (!td) {
            std::cout << "SKIP: serialization checks because TypeDescriptor was not available\n";
            serialization_ok = false;
        }
        else {
            try {
                // Create sample object and populate via reflection
                T src{};
                auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
                if (!sdesc) {
                    std::cout << "FAIL: not a struct descriptor; cannot serialize\n";
                    serialization_ok = false;
                }
                else {
                    if (sdesc->members.size() >= 3) {
                        *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&src)) = 10.0f;
                        *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&src)) = -3.5f;
                        *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&src)) = 0.25f;
                    }
                    else {
                        std::cout << "  WARN: not enough members to populate canonical values\n";
                    }

                    // 1) Text Serialize
                    std::stringstream ss;
                    td->Serialize(&src, ss);
                    std::string text_out = ss.str();
                    std::cout << "  Text Serialize output: " << text_out << "\n";

                    // 2) rapidjson SerializeJson -> string
                    rapidjson::Document dout;
                    td->SerializeJson(&src, dout);
                    rapidjson::StringBuffer sb;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
                    dout.Accept(writer);
                    std::cout << "  rapidjson Serialize output: " << sb.GetString() << "\n";

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

                    std::cout << "  Round-trip equality: " << (match ? "OK" : "MISMATCH") << "\n";
                    if (!match) serialization_ok = false;
                }
            }
            catch (const std::exception& ex) {
                std::cout << "FAIL: exception during serialization tests: " << ex.what() << "\n";
                serialization_ok = false;
            }
            catch (...) {
                std::cout << "FAIL: unknown error during serialization tests\n";
                serialization_ok = false;
            }
        }

        // --- Registry introspection (optional) ---
        std::cout << "\n[3] Registry contents (keys):\n";
        for (const auto& kv : TypeDescriptor::type_descriptor_lookup()) {
            std::cout << "  " << kv.first << "\n";
        }

        // --- Summary & exit code ---
        std::cout << "\n=== SUMMARY ===\n";
        std::cout << "Reflection: " << (reflection_ok ? "PASS" : "FAIL") << "\n";
        std::cout << "Serialization: " << (serialization_ok ? "PASS" : "FAIL") << "\n";

        if (!reflection_ok) {
            std::cout << R"(
NOTE: if you hit a linker error mentioning GetPrimitiveDescriptor<float&>() or you see member types printed with '&',
apply the macro fix to strip references when resolving member types in the macro:

Replace the TypeResolver line in REFL_REGISTER_PROPERTY with:
  TypeResolver<std::remove_reference_t<decltype(std::declval<T>().VARIABLE)>>::Get()

This prevents requesting descriptors for reference types (e.g. float&).
)";
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
			ENGINE_LOG_ERROR("Failed to initialize AudioManager");
		}
		else
		{
			AudioManager::StaticLoadSound("test_sound", "Test_duck.wav", false);
			AudioManager::StaticPlaySound("test_sound", 0.5f, 1.0f);
		}
	}

	ENGINE_LOG_INFO("Engine initialization completed successfully");
	
	// Add some test logging messages
	ENGINE_LOG_WARN("This is a test warning message");
	ENGINE_LOG_ERROR("This is a test error message");
	
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
    std::cout << "[Engine] Shutdown complete" << std::endl;
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