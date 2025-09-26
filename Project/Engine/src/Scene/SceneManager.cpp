#include "pch.h"
#include <ECS/ECSRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include <Scene/SceneInstance.hpp>
#include <filesystem>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <ECS/NameComponent.hpp>

ENGINE_API SceneManager::~SceneManager() {
	ExitScene();
}

// Temporary function to load the test scene.
void SceneManager::LoadTestScene() {
	ECSRegistry::GetInstance().CreateECSManager("TestScene");
	currentScene = std::make_unique<SceneInstance>("TestScene");
	currentScenePath = "TestScene";
	currentScene->Initialize();
}

// Load a new scene from the specified path.
// The current scene is exited and cleaned up before loading the new scene.
// Also sets the new scene as the active ECSManager in the ECSRegistry.
ENGINE_API void SceneManager::LoadScene(const std::string& scenePath) {
	// Exit and clean up the current scene if it exists.
	if (currentScene) 
    {
		currentScene->Exit();

		ECSRegistry::GetInstance().GetECSManager(currentScenePath).ClearAllEntities();
		ECSRegistry::GetInstance().RenameECSManager(currentScenePath, scenePath);
	}

	// Create and initialize the new scene.
	currentScene = std::make_unique<SceneInstance>(scenePath);
	currentScenePath = scenePath;

	// Deserialize the new scene data.
	//Serializer::GetInstance().DeserializeScene(scenePath);
    // 
	// Initialize the new scene.
	currentScene->Initialize();
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

//Simple JSON save scene
ENGINE_API void SceneManager::SaveScene()
{
    namespace fs = std::filesystem;

    // choose output path: prefer currentScenePath if available, else use "scene.json"
    std::string outPath = "Resources/Scene/scene.json";
    currentScenePath = "Resources/Scene/scene.json";

    // Prepare JSON document
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value entitiesArr(rapidjson::kArrayType);

    // helper lambda to serialize a component instance (via reflection) into a rapidjson::Value
    auto serializeComponentToValue = [&](auto& compInstance) -> rapidjson::Value {
        using CompT = std::decay_t<decltype(compInstance)>;
        rapidjson::Value val; val.SetNull();

        try {
            TypeDescriptor* td = TypeResolver<CompT>::Get();
            std::stringstream ss;
            td->Serialize(&compInstance, ss);
            std::string s = ss.str();

            // parse the serialized string to a temporary document
            rapidjson::Document tmp;
            if (tmp.Parse(s.c_str()).HasParseError()) {
                // If parse fails, store the raw string instead
                rapidjson::Value strVal;
                strVal.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
                val = strVal;
            }
            else {
                // copy tmp into val using allocator
                val.CopyFrom(tmp, alloc);
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "[SaveScene] reflection serialize exception: " << ex.what() << "\n";
            // leave val as null
        }
        catch (...) {
            std::cerr << "[SaveScene] unknown exception during component serialization\n";
        }

        return val;
        };

    // Get ECS manager (guard in case there's no active manager)
    ECSManager* ecsPtr = nullptr;
    try {
        ecsPtr = &ECSRegistry::GetInstance().GetActiveECSManager();
    }
    catch (...) {
        std::cerr << "[SaveScene] no active ECSManager available; aborting save\n";
        return;
    }
    ECSManager& ecs = *ecsPtr;

    // Iterate entities
    for (auto entity : ecs.GetAllEntities())
    {
        rapidjson::Value entObj(rapidjson::kObjectType);

        // add entity id (assumes entity is integer-like)
        {
            rapidjson::Value idv;
            idv.SetUint64(static_cast<uint64_t>(entity)); // adapt if entity type differs
            entObj.AddMember("id", idv, alloc);
        }

        rapidjson::Value compsObj(rapidjson::kObjectType);

        // For each component type, if entity has it, serialize and attach under its name
        if (ecs.HasComponent<NameComponent>(entity)) {
            auto& c = ecs.GetComponent<NameComponent>(entity);

            // Build { "name": "<the name>" } object
            rapidjson::Value nameObj(rapidjson::kObjectType);
            rapidjson::Value nameStr;
            nameStr.SetString(c.name.c_str(),
                static_cast<rapidjson::SizeType>(c.name.size()),
                alloc);
            nameObj.AddMember(rapidjson::Value("name", alloc).Move(), nameStr, alloc);

            // Add under "NameComponent" key (ensure key uses allocator)
            compsObj.AddMember(rapidjson::Value("NameComponent", alloc).Move(),
                nameObj,
                alloc);
        }
        if (ecs.HasComponent<Transform>(entity)) {
            auto& c = ecs.GetComponent<Transform>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("Transform", v, alloc);
        }
        if (ecs.HasComponent<ModelRenderComponent>(entity)) {
            auto& c = ecs.GetComponent<ModelRenderComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ModelRenderComponent", v, alloc);
        }
        if (ecs.HasComponent<DebugDrawComponent>(entity)) {
            auto& c = ecs.GetComponent<DebugDrawComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("DebugDrawComponent", v, alloc);
        }
        if (ecs.HasComponent<ChildrenComponent>(entity)) {
            auto& c = ecs.GetComponent<ChildrenComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ChildrenComponent", v, alloc);
        }
        if (ecs.HasComponent<TextRenderComponent>(entity)) {
            auto& c = ecs.GetComponent<TextRenderComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("TextRenderComponent", v, alloc);
        }
        if (ecs.HasComponent<ParentComponent>(entity)) {
            auto& c = ecs.GetComponent<ParentComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ParentComponent", v, alloc);
        }

        entObj.AddMember("components", compsObj, alloc);
        entitiesArr.PushBack(entObj, alloc);
    }

    doc.AddMember("entities", entitiesArr, alloc);

    // Write to file (ensure parent directory exists; fallback to current directory if creation fails)
    {
        fs::path outPathP(outPath);

        // Try to create parent directories if needed
        if (outPathP.has_parent_path()) {
            fs::path parent = outPathP.parent_path();
            std::error_code ec;
            if (!fs::exists(parent)) {
                if (!fs::create_directories(parent, ec)) {
                    std::cerr << "[SaveScene] failed to create directory '" << parent.string()
                        << "': " << ec.message() << "\n"
                        << "[SaveScene] will attempt to write to current working directory instead.\n";
                    // fallback: use filename only
                    outPathP = outPathP.filename();
                }
            }
        }

        // Prepare buffer & writer
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        // Try to open the file (this will create the file if it doesn't exist)
        std::ofstream ofs(outPathP.string(), std::ios::binary | std::ios::trunc);
        if (!ofs) {
            std::cerr << "[SaveScene] failed to open output file: " << outPathP.string() << "\n";
            return;
        }

        ofs << buffer.GetString();
        ofs.close();

        std::cout << "[SaveScene] wrote scene JSON to: " << outPathP.string()
            << " (" << buffer.GetSize() << " bytes)\n";
    }
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
		std::cerr << "Temp file does not exist: " << tempScenePath << std::endl;
		return; // Early exit if needed
	}
}
