#include "pch.h"
#include <Scene/SceneInstance.hpp>
#include <Input/InputManager.hpp>
#include <Input/Keys.h>
#include <WindowManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include <Asset Manager/AssetManager.hpp>
#include "TimeManager.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Transform/TransformComponent.hpp>
#include <Graphics/TextRendering/TextUtils.hpp>
#include "ECS/NameComponent.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif

void SceneInstance::Initialize() {
	// Initialize GraphicsManager first
	GraphicsManager& gfxManager = GraphicsManager::GetInstance();
	//gfxManager.Initialize(WindowManager::GetWindowWidth(), WindowManager::GetWindowHeight());
	gfxManager.Initialize(RunTimeVar::window.width, RunTimeVar::window.height);
	// WOON LI TEST CODE
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetECSManager(scenePath);

#if 1
	// Create a backpack entity with a Renderer component in the main ECS manager
	Entity backpackEntt = ecsManager.CreateEntity();
	ecsManager.transformSystem->SetLocalPosition(backpackEntt, { 0, 0, 0 });
	ecsManager.transformSystem->SetLocalScale(backpackEntt, { .1f, .1f, .1f });
	ecsManager.transformSystem->SetLocalRotation(backpackEntt, { 0, 0, 0 });
	NameComponent& backpackName = ecsManager.GetComponent<NameComponent>(backpackEntt);
	backpackName.name = "dora the explorer";
	ecsManager.AddComponent<ModelRenderComponent>(backpackEntt, ModelRenderComponent{ GUIDUtilities::ConvertStringToGUID128("00305bf2963148e7-0002b9781e000005"), GUIDUtilities::ConvertStringToGUID128("00789a0240b76981-0002b65c12000013")});
	//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
	//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});

	Entity backpackEntt2 = ecsManager.CreateEntity();
	ecsManager.transformSystem->SetLocalPosition(backpackEntt2, { 1, -0.5f, 0 });
	ecsManager.transformSystem->SetLocalScale(backpackEntt2, { .2f, .2f, .2f });
	ecsManager.transformSystem->SetLocalRotation(backpackEntt2, { 0, 0, 0 });
	NameComponent& backpack2Name = ecsManager.GetComponent<NameComponent>(backpackEntt2);
	backpack2Name.name = "ash ketchum";
	ecsManager.AddComponent<ModelRenderComponent>(backpackEntt2, ModelRenderComponent{ GUIDUtilities::ConvertStringToGUID128("00305bf2963148e7-0002b9781e000005"), GUIDUtilities::ConvertStringToGUID128("00789a0240b76981-0002b65c12000013") });
	//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt2, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
	//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});

	Entity backpackEntt3 = ecsManager.CreateEntity();
	ecsManager.transformSystem->SetLocalPosition(backpackEntt3, { -2, 0.5f, 0 });
	ecsManager.transformSystem->SetLocalScale(backpackEntt3, { .5f, .5f, .5f });
	ecsManager.transformSystem->SetLocalRotation(backpackEntt3, { 50, 70, 20 });
	NameComponent& backpack3Name = ecsManager.GetComponent<NameComponent>(backpackEntt3);
	backpack3Name.name = "indiana jones";
	ecsManager.AddComponent<ModelRenderComponent>(backpackEntt3, ModelRenderComponent{ GUIDUtilities::ConvertStringToGUID128("00305bf2963148e7-0002b9781e000005"), GUIDUtilities::ConvertStringToGUID128("00789a0240b76981-0002b65c12000013") });
	//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt3, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
	//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});

	// Text entity test
	Entity text = ecsManager.CreateEntity();
	ecsManager.GetComponent<NameComponent>(text).name = "Text1";
	ecsManager.AddComponent<TextRenderComponent>(text, TextRenderComponent{ "hello woody", 48, GUIDUtilities::ConvertStringToGUID128("00305bf28c15852e-0002b9781c000001"), GUIDUtilities::ConvertStringToGUID128("00789a027b8447e2-0002b65c12000017") });
	//ecsManager.AddComponent<TextRenderComponent>(text, TextRenderComponent{ "Hello World!", ResourceManager::GetInstance().GetFontResource("Resources/Fonts/Kenney Mini.ttf"), ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("text")) });
	TextRenderComponent& textComp = ecsManager.GetComponent<TextRenderComponent>(text);
	TextUtils::SetPosition(textComp, Vector3D(800, 100, 0));
	TextUtils::SetAlignment(textComp, TextRenderComponent::Alignment::CENTER);

	//Entity text2 = ecsManager.CreateEntity();
	//ecsManager.GetComponent<NameComponent>(text2).name = "Text2";
	//ecsManager.AddComponent<TextRenderComponent>(text2, TextRenderComponent{ "woohoo?", ResourceManager::GetInstance().GetFontResource("Resources/Fonts/Kenney Mini.ttf", 20), ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("text")) });
	//TextRenderComponent& textComp2 = ecsManager.GetComponent<TextRenderComponent>(text2);
	//TextUtils::SetPosition(textComp2, Vector3D(800, 800, 0));
	//TextUtils::SetAlignment(textComp2, TextRenderComponent::Alignment::CENTER);
	// 
	// Creates light
	lightShader = std::make_shared<Shader>();
	lightShader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("light"));
	//lightShader->LoadAsset("Resources/Shaders/light");
	std::vector<std::shared_ptr<Texture>> emptyTextures = {};
	lightCubeMesh = std::make_shared<Mesh>(lightVertices, lightIndices, emptyTextures);

	// Sets camera
	gfxManager.SetCamera(&camera);

	// Initialize systems.
	ecsManager.transformSystem->Initialise();
	ecsManager.modelSystem->Initialise();
	ecsManager.debugDrawSystem->Initialise();
	ecsManager.textSystem->Initialise();

	std::cout << "TestScene Initialized" << std::endl;
}

void SceneInstance::Initialize(const std::string& scenePathArg)
{
	// set the internal scenePath if provided (so CreateEntitiesFromJson uses it)
	if (!scenePathArg.empty()) {
		scenePath = scenePathArg;
	}

	// First run the no-arg initialization (graphics/systems)
	Initialize();

	// Then create entities from JSON (if any file exists)
	std::filesystem::path jsonPath = std::filesystem::path(scenePath.empty() ? "scene.json" : (scenePath + ".json"));
	CreateEntitiesFromJson(jsonPath);
}

// Private helper: parse JSON & create entities (keeps most of the logic from previous loader)
void SceneInstance::CreateEntitiesFromJson(const std::filesystem::path& inPath)
{
    using namespace std;
    namespace fs = std::filesystem;

    fs::path pathToOpen = inPath;
    if (!fs::exists(pathToOpen)) {
        // fallback to filename-only (in case save used only filename)
        pathToOpen = pathToOpen.filename();
    }

    if (!fs::exists(pathToOpen)) {
        std::cerr << "[CreateEntitiesFromJson] no scene JSON file found: " << inPath.string() << "\n";
        return;
    }

    std::ifstream ifs(pathToOpen.string(), std::ios::binary);
    if (!ifs) {
        std::cerr << "[CreateEntitiesFromJson] failed to open: " << pathToOpen.string() << "\n";
        return;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string jsonStr = ss.str();
    ifs.close();

    rapidjson::Document doc;
    if (doc.Parse(jsonStr.c_str()).HasParseError()) {
        std::cerr << "[CreateEntitiesFromJson] JSON parse error in: " << pathToOpen.string() << "\n";
        return;
    }

    if (!doc.HasMember("entities") || !doc["entities"].IsArray()) {
        std::cerr << "[CreateEntitiesFromJson] no entities array in JSON: " << pathToOpen.string() << "\n";
        return;
    }

    ECSManager& ecs = ECSRegistry::GetInstance().GetECSManager(scenePath);

    // map oldId -> new entity
    std::unordered_map<uint64_t, Entity> idMap;
    struct PendingParent { uint64_t childOld; uint64_t parentOld; };
    std::vector<PendingParent> pendingParents;
    struct PendingChildren { uint64_t parentOld; std::vector<uint64_t> childOlds; };
    std::vector<PendingChildren> pendingChildrenSets;

    // ---------- helpers ----------
    auto readVec3FromArray = [](const rapidjson::Value& a, Vector3D& out) -> bool {
        if (!a.IsArray() || a.Size() < 3) return false;
        out.x = static_cast<float>(a[0].GetDouble());
        out.y = static_cast<float>(a[1].GetDouble());
        out.z = static_cast<float>(a[2].GetDouble());
        return true;
        };

    auto getNumberFromValue = [](const rapidjson::Value& v, double& out) -> bool {
        if (v.IsNumber()) { out = v.GetDouble(); return true; }
        if (v.IsObject() && v.HasMember("data")) {
            const auto& d = v["data"];
            if (d.IsNumber()) { out = d.GetDouble(); return true; }
            if (d.IsObject() && d.HasMember("data") && d["data"].IsNumber()) { out = d["data"].GetDouble(); return true; }
        }
        return false;
        };

    auto getBoolFromValue = [](const rapidjson::Value& v, bool& out) -> bool {
        if (v.IsBool()) { out = v.GetBool(); return true; }
        if (v.IsObject() && v.HasMember("data") && v["data"].IsBool()) { out = v["data"].GetBool(); return true; }
        return false;
        };

    auto getStringFromValue = [](const rapidjson::Value& v, std::string& out) -> bool {
        if (v.IsString()) { out = v.GetString(); return true; }
        if (v.IsObject() && v.HasMember("data") && v["data"].IsString()) { out = v["data"].GetString(); return true; }
        return false;
        };

    // read a Vector3D stored as either [x,y,z] or typed {"type":"Vector3D","data":[{...},{...},{...}]}
    auto readVec3Generic = [&](const rapidjson::Value& val, Vector3D& out)->bool {
        if (val.IsArray()) return readVec3FromArray(val, out);
        if (val.IsObject() && val.HasMember("data") && val["data"].IsArray()) {
            const auto& darr = val["data"];
            if (darr.Size() >= 3) {
                double x, y, z;
                if (getNumberFromValue(darr[0], x) && getNumberFromValue(darr[1], y) && getNumberFromValue(darr[2], z)) {
                    out.x = static_cast<float>(x);
                    out.y = static_cast<float>(y);
                    out.z = static_cast<float>(z);
                    return true;
                }
            }
        }
        return false;
        };

    // read quaternion stored as typed object or plain array [w,x,y,z]
    auto readQuatGeneric = [&](const rapidjson::Value& val, double& outW, double& outX, double& outY, double& outZ)->bool {
        if (val.IsArray() && val.Size() >= 4) {
            outW = val[0].GetDouble();
            outX = val[1].GetDouble();
            outY = val[2].GetDouble();
            outZ = val[3].GetDouble();
            return true;
        }
        if (val.IsObject() && val.HasMember("data") && val["data"].IsArray()) {
            const auto& darr = val["data"];
            if (darr.Size() >= 4) {
                if (getNumberFromValue(darr[0], outW) && getNumberFromValue(darr[1], outX)
                    && getNumberFromValue(darr[2], outY) && getNumberFromValue(darr[3], outZ)) {
                    return true;
                }
            }
        }
        return false;
        };

    // convert quaternion (w,x,y,z) -> Euler degrees (roll, pitch, yaw) in Vector3D (x=roll, y=pitch, z=yaw)
    auto quatToEulerDeg = [](double w, double x, double y, double z) -> Vector3D {
        // safe pi
        const double PI = std::acos(-1.0);
        // roll (x-axis rotation)
        double sinr_cosp = 2.0 * (w * x + y * z);
        double cosr_cosp = 1.0 - 2.0 * (x * x + y * y);
        double roll = std::atan2(sinr_cosp, cosr_cosp);

        // pitch (y-axis rotation)
        double sinp = 2.0 * (w * y - z * x);
        double pitch;
        if (std::abs(sinp) >= 1)
            pitch = std::copysign(PI / 2.0, sinp); // use 90 degrees if out of range
        else
            pitch = std::asin(sinp);

        // yaw (z-axis rotation)
        double siny_cosp = 2.0 * (w * z + x * y);
        double cosy_cosp = 1.0 - 2.0 * (y * y + z * z);
        double yaw = std::atan2(siny_cosp, cosy_cosp);

        const double RAD2DEG = 180.0 / PI;
        return Vector3D{
            static_cast<float>(roll * RAD2DEG),
            static_cast<float>(pitch * RAD2DEG),
            static_cast<float>(yaw * RAD2DEG)
        };
        };

    // ---------- end helpers ----------

    const rapidjson::Value& ents = doc["entities"];
    for (rapidjson::SizeType i = 0; i < ents.Size(); ++i) {
        const rapidjson::Value& entObj = ents[i];
        if (!entObj.IsObject()) continue;

        uint64_t oldId = 0;
        if (entObj.HasMember("id") && entObj["id"].IsUint64()) oldId = entObj["id"].GetUint64();
        else if (entObj.HasMember("id") && entObj["id"].IsUint()) oldId = static_cast<uint64_t>(entObj["id"].GetUint());

        Entity newEnt = ecs.CreateEntity();
        idMap[oldId] = newEnt;

        if (!entObj.HasMember("components") || !entObj["components"].IsObject()) continue;
        const rapidjson::Value& comps = entObj["components"];

        // NameComponent
        if (comps.HasMember("NameComponent")) {
            const rapidjson::Value& nv = comps["NameComponent"];
            std::string name;
            if (nv.IsObject() && nv.HasMember("name") && nv["name"].IsString()) name = nv["name"].GetString();
            else if (nv.IsString()) name = nv.GetString();
            else if (nv.IsObject() && nv.HasMember("data")) getStringFromValue(nv["data"], name);

            if (!name.empty()) {
                try {
                    ecs.GetComponent<NameComponent>(newEnt).name = name;
                }
                catch (...) {
                    NameComponent nc; nc.name = name;
                    ecs.AddComponent<NameComponent>(newEnt, nc);
                }
            }
        }

        // Transform
        if (comps.HasMember("Transform") && comps["Transform"].IsObject()) {
            const rapidjson::Value& t = comps["Transform"];
            Vector3D pos{ 0.f,0.f,0.f }, rot{ 0.f,0.f,0.f }, scale{ 1.f,1.f,1.f };

            // legacy simple arrays
            if (t.HasMember("position") && t["position"].IsArray()) readVec3FromArray(t["position"], pos);
            else if (t.HasMember("localPosition") && t["localPosition"].IsArray()) readVec3FromArray(t["localPosition"], pos);

            if (t.HasMember("rotation") && t["rotation"].IsArray()) readVec3FromArray(t["rotation"], rot);
            else if (t.HasMember("localRotation") && t["localRotation"].IsArray()) readVec3FromArray(t["localRotation"], rot);

            if (t.HasMember("scale") && t["scale"].IsArray()) readVec3FromArray(t["scale"], scale);
            else if (t.HasMember("localScale") && t["localScale"].IsArray()) readVec3FromArray(t["localScale"], scale);

            // typed Transform (your format): data is an array; observed order in your file is: position, scale, rotation(quaternion), ...
            if (t.HasMember("data") && t["data"].IsArray()) {
                const auto& darr = t["data"];
                if (darr.Size() > 0) {
                    readVec3Generic(darr[0], pos); // index 0 -> position
                }
                if (darr.Size() > 1) {
                    Vector3D maybeScale{ 1.f,1.f,1.f };
                    if (readVec3Generic(darr[1], maybeScale)) scale = maybeScale; // index 1 -> scale
                }
                if (darr.Size() > 2) {
                    double w = 1, x = 0, y = 0, z = 0;
                    if (readQuatGeneric(darr[2], w, x, y, z)) { // index 2 -> quaternion
                        rot = quatToEulerDeg(w, x, y, z);
                    }
                }
            }

            if (ecs.transformSystem) {
                ecs.transformSystem->SetLocalPosition(newEnt, pos);
                ecs.transformSystem->SetLocalRotation(newEnt, rot); // expects Euler degrees in your code
                ecs.transformSystem->SetLocalScale(newEnt, scale);
            }
            else
            {
                Transform tf; tf.localPosition = pos; tf.localRotation = Quaternion::FromEulerDegrees(rot); tf.localScale = scale;
                ecs.AddComponent<Transform>(newEnt, tf);
            }
        }

        // ModelRenderComponent
        if (comps.HasMember("ModelRenderComponent")) {
            const rapidjson::Value& mv = comps["ModelRenderComponent"];
            std::string modelPath; std::string shaderPath;
            bool enabledFromFile = false;

            if (mv.IsObject()) {
                if (mv.HasMember("modelPath") && mv["modelPath"].IsString()) modelPath = mv["modelPath"].GetString();
                else if (mv.HasMember("model") && mv["model"].IsString()) modelPath = mv["model"].GetString();
                if (mv.HasMember("shaderPath") && mv["shaderPath"].IsString()) shaderPath = mv["shaderPath"].GetString();

                if (mv.HasMember("data") && mv["data"].IsArray() && mv["data"].Size() > 0) {
                    bool b;
                    if (getBoolFromValue(mv["data"][0], b)) enabledFromFile = b;
                }
            }
            else if (mv.IsString()) {
                modelPath = mv.GetString();
            }

            if (!modelPath.empty()) {
                ModelRenderComponent mrc;
                mrc.model = ResourceManager::GetInstance().GetResource<Model>(modelPath);
                if (!shaderPath.empty()) mrc.shader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);
                else mrc.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"));
                // optional: set enabled flag if your component supports it, using enabledFromFile
                ecs.AddComponent<ModelRenderComponent>(newEnt, mrc);
            }
        }

        // TextRenderComponent
        if (comps.HasMember("TextRenderComponent") && comps["TextRenderComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["TextRenderComponent"];
            TextRenderComponent trc;
            int fontSize = 16;

            // legacy keys
            if (tv.HasMember("text") && tv["text"].IsString()) trc.text = tv["text"].GetString();
            if (tv.HasMember("font") && tv["font"].IsString()) {
                std::string fontPath = tv["font"].GetString();
                if (tv.HasMember("fontSize") && tv["fontSize"].IsInt()) fontSize = tv["fontSize"].GetInt();
                trc.font = ResourceManager::GetInstance().GetFontResource(fontPath, fontSize);
            }
            else {
                trc.font = ResourceManager::GetInstance().GetFontResource("Resources/Fonts/Kenney Mini.ttf", fontSize);
            }

            // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
            if (tv.HasMember("data") && tv["data"].IsArray()) {
                const auto& d = tv["data"];
                if (d.Size() > 0) {
                    std::string s;
                    if (getStringFromValue(d[0], s)) trc.text = s;
                }
                if (d.Size() > 1) {
                    double fsd;
                    if (getNumberFromValue(d[1], fsd)) fontSize = static_cast<int>(fsd);
                }
                // update font now that we have fontSize (still using default font path unless 'font' key present)
                trc.font = ResourceManager::GetInstance().GetFontResource("Resources/Fonts/Kenney Mini.ttf", fontSize);
            }

            trc.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("text"));
            ecs.AddComponent<TextRenderComponent>(newEnt, trc);

            // optional position/alignment restoration (legacy)
            if (tv.HasMember("position") && tv["position"].IsArray()) {
                Vector3D pos; readVec3FromArray(tv["position"], pos);
                TextUtils::SetPosition(ecs.GetComponent<TextRenderComponent>(newEnt), pos.x, pos.y, pos.z);
            }
            if (tv.HasMember("alignment") && tv["alignment"].IsString()) {
                std::string a = tv["alignment"].GetString();
                if (a == "CENTER") TextUtils::SetAlignment(ecs.GetComponent<TextRenderComponent>(newEnt), TextRenderComponent::Alignment::CENTER);
                else if (a == "LEFT") TextUtils::SetAlignment(ecs.GetComponent<TextRenderComponent>(newEnt), TextRenderComponent::Alignment::LEFT);
                else if (a == "RIGHT") TextUtils::SetAlignment(ecs.GetComponent<TextRenderComponent>(newEnt), TextRenderComponent::Alignment::RIGHT);
            }
        }

        // Parent / Children -> store to apply in second pass
        if (comps.HasMember("ParentComponent")) {
            const rapidjson::Value& pv = comps["ParentComponent"];
            uint64_t parentOldId = 0;
            if (pv.IsObject() && pv.HasMember("parent") && pv["parent"].IsUint64()) parentOldId = pv["parent"].GetUint64();
            else if (pv.IsUint64()) parentOldId = pv.GetUint64();
            else if (pv.IsObject() && pv.HasMember("parent") && pv["parent"].IsUint()) parentOldId = static_cast<uint64_t>(pv["parent"].GetUint());
            else if (pv.IsObject() && pv.HasMember("data") && pv["data"].IsArray() && pv["data"].Size() > 0) {
                // typed form where parent might be inside data array
                const auto& d0 = pv["data"][0];
                if (d0.IsUint64()) parentOldId = d0.GetUint64();
                else if (d0.IsUint()) parentOldId = static_cast<uint64_t>(d0.GetUint());
            }
            if (parentOldId != 0) pendingParents.push_back({ oldId, parentOldId });
        }

        if (comps.HasMember("ChildrenComponent")) {
            const rapidjson::Value& cv = comps["ChildrenComponent"];
            if (cv.IsObject() && cv.HasMember("children") && cv["children"].IsArray()) {
                std::vector<uint64_t> childIds;
                for (auto& cval : cv["children"].GetArray()) {
                    if (cval.IsUint64()) childIds.push_back(cval.GetUint64());
                    else if (cval.IsUint()) childIds.push_back(static_cast<uint64_t>(cval.GetUint()));
                    else if (cval.IsObject() && cval.HasMember("data") && cval["data"].IsArray() && cval["data"].Size() > 0) {
                        const auto& d0 = cval["data"][0];
                        if (d0.IsUint64()) childIds.push_back(d0.GetUint64());
                        else if (d0.IsUint()) childIds.push_back(static_cast<uint64_t>(d0.GetUint()));
                    }
                }
                if (!childIds.empty()) pendingChildrenSets.push_back({ oldId, std::move(childIds) });
            }
            // typed fallback: ChildrenComponent.data = [ {type:"...","data":[id1,...]} ]
            else if (cv.IsObject() && cv.HasMember("data") && cv["data"].IsArray()) {
                std::vector<uint64_t> childIds;
                for (auto& dval : cv["data"].GetArray()) {
                    // if each entry is an object with data array containing the ids
                    if (dval.IsArray()) {
                        for (auto& v : dval.GetArray()) {
                            if (v.IsUint64()) childIds.push_back(v.GetUint64());
                            else if (v.IsUint()) childIds.push_back(static_cast<uint64_t>(v.GetUint()));
                        }
                    }
                    else if (dval.IsObject() && dval.HasMember("data") && dval["data"].IsArray()) {
                        for (auto& v : dval["data"].GetArray()) {
                            if (v.IsUint64()) childIds.push_back(v.GetUint64());
                            else if (v.IsUint()) childIds.push_back(static_cast<uint64_t>(v.GetUint()));
                        }
                    }
                }
                if (!childIds.empty()) pendingChildrenSets.push_back({ oldId, std::move(childIds) });
            }
        }

    } // end for entities

    // Second pass: apply parents/children
    //auto getNew = [&](uint64_t old) -> Entity {
    //    auto it = idMap.find(old);
    //    if (it != idMap.end()) return it->second;
    //    return Entity{}; // default / null entity
    //    };

    //for (const auto& p : pendingParents) {
    //    Entity child = getNew(p.childOld);
    //    Entity parent = getNew(p.parentOld);
    //    if (!child || !parent) continue;
    //    ParentComponent pc; pc.parent = parent;
    //    ecs.AddComponent<ParentComponent>(child, pc);
    //    if (!ecs.HasComponent<ChildrenComponent>(parent)) {
    //        ChildrenComponent cc; cc.children.clear();
    //        ecs.AddComponent<ChildrenComponent>(parent, cc);
    //    }
    //    auto& ccRef = ecs.GetComponent<ChildrenComponent>(parent);
    //    ccRef.children.push_back(child);
    //}

    //for (const auto& cs : pendingChildrenSets) {
    //    Entity parent = getNew(cs.parentOld);
    //    if (!parent) continue;
    //    if (!ecs.HasComponent<ChildrenComponent>(parent)) {
    //        ChildrenComponent cc; cc.children.clear();
    //        ecs.AddComponent<ChildrenComponent>(parent, cc);
    //    }
    //    auto& ccRef = ecs.GetComponent<ChildrenComponent>(parent);
    //    for (auto childOld : cs.childOlds) {
    //        Entity child = getNew(childOld);
    //        if (!child) continue;
    //        ccRef.children.push_back(child);
    //        ParentComponent pc; pc.parent = parent;
    //        ecs.AddComponent<ParentComponent>(child, pc);
    //    }
    //}

    std::cout << "[CreateEntitiesFromJson] loaded entities from: " << pathToOpen.string() << " (created: " << idMap.size() << ")\n";
}

void SceneInstance::Update(double dt) {
	dt;

	// Update logic for the test scene
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	processInput((float)TimeManager::GetDeltaTime());

	// Update systems.
	mainECS.transformSystem->Update();
}

void SceneInstance::Draw() {
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	GraphicsManager& gfxManager = GraphicsManager::GetInstance();
	//RenderSystem::getInstance().BeginFrame();
	gfxManager.BeginFrame();
	gfxManager.Clear();

	//glm::mat4 transform = glm::mat4(1.0f);
	//transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.0f));
	//transform = glm::scale(transform, glm::vec3(0.1f, 0.1f, 0.1f));
	//RenderSystem::getInstance().Submit(backpackModel, transform, shader);

	gfxManager.SetCamera(&camera);
	if (mainECS.modelSystem)
	{
		mainECS.modelSystem->Update();
	}
	if (mainECS.textSystem)
	{
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call textSystem->Update()");
#endif
		mainECS.textSystem->Update();
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "textSystem->Update() completed");
#endif
	}
	// Test debug drawing
	DebugDrawSystem::DrawCube(Vector3D(0, 1, 0), Vector3D(1, 1, 1), Vector3D(1, 0, 0)); // Red cube above origin
	DebugDrawSystem::DrawSphere(Vector3D(2, 0, 0), 1.0f, Vector3D(0, 1, 0)); // Green sphere to the right
	DebugDrawSystem::DrawLine(Vector3D(0, 0, 0), Vector3D(3, 3, 3), Vector3D(0, 0, 1)); // Blue line diagonal
	auto backpackModel = ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj");
	DebugDrawSystem::DrawMeshWireframe(backpackModel, Vector3D(-2, 0, 0), Vector3D(1, 1, 0), 0.0f);

	// Update debug draw system to submit to graphics manager
	if (mainECS.debugDrawSystem)
	{
		mainECS.debugDrawSystem->Update();
	}
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call gfxManager.Render()");
#endif
	gfxManager.Render();
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "gfxManager.Render() completed");
#endif

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call DrawLightCubes()");
#endif
	// 5. Draw light cubes manually (temporary - you can make this a system later)
	DrawLightCubes();
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() completed");
#endif

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call gfxManager.EndFrame()");
#endif
	// 6. End frame
	gfxManager.EndFrame();
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "gfxManager.EndFrame() completed");
#endif

//std::cout << "drawn\n";
}

void SceneInstance::Exit() {
	// Cleanup code for the test scene

	// Exit systems.
	//ECSRegistry::GetInstance().GetECSManager(scenePath).modelSystem->Exit();

	std::cout << "TestScene Exited" << std::endl;
}

void SceneInstance::processInput(float deltaTime)
{
	if (InputManager::GetKeyDown(Input::Key::ESC))
		WindowManager::SetWindowShouldClose();

	float cameraSpeed = 2.5f * deltaTime;
	if (InputManager::GetKey(Input::Key::W))
		camera.Position += cameraSpeed * camera.Front;
	if (InputManager::GetKey(Input::Key::S))
		camera.Position -= cameraSpeed * camera.Front;
	if (InputManager::GetKey(Input::Key::A))
		camera.Position -= glm::normalize(glm::cross(camera.Front, camera.Up)) * cameraSpeed;
	if (InputManager::GetKey(Input::Key::D))
		camera.Position += glm::normalize(glm::cross(camera.Front, camera.Up)) * cameraSpeed;

	float xpos = (float)InputManager::GetMouseX();
	float ypos = (float)InputManager::GetMouseY();

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void SceneInstance::DrawLightCubes()
{
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() - checking lightShader");
#endif

	// Check if lightShader is valid (asset loading might have failed on Android)
	if (!lightShader) {
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_WARN, "GAM300", "DrawLightCubes() - lightShader is null, skipping");
#endif
		return;
	}

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() - lightShader is valid");
#endif

	// Get light positions from LightManager instead of renderSystem
	LightManager& lightManager = LightManager::getInstance();
	const auto& pointLights = lightManager.getPointLights();

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() - about to loop through %zu lights", pointLights.size());
#endif

	// Draw light cubes at point light positions
	for (size_t i = 0; i < pointLights.size() && i < 4; i++) {
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() - processing light %zu", i);
#endif
		lightShader->Activate();

		// Set up matrices for light cube
		glm::mat4 lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, pointLights[i].position);
		lightModel = glm::scale(lightModel, glm::vec3(0.2f)); // Make them smaller

		// Set up view and projection matrices
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 projection = glm::perspective(
			glm::radians(camera.Zoom),
			//(float)WindowManager::GetWindowWidth() / (float)WindowManager::GetWindowHeight(),
			(float)RunTimeVar::window.width / (float)RunTimeVar::window.height,
			0.1f, 100.0f
		);

		lightShader->setMat4("model", lightModel);
		lightShader->setMat4("view", view);
		lightShader->setMat4("projection", projection);
		//lightShader->setVec3("lightColor", pointLights[i].diffuse); // Use light color

		lightCubeMesh->Draw(*lightShader, camera);
	}
}

void SceneInstance::DrawLightCubes(const Camera& cameraOverride)
{
	// Get light positions from LightManager instead of renderSystem
	LightManager& lightManager = LightManager::getInstance();
	const auto& pointLights = lightManager.getPointLights();

	// Draw light cubes at point light positions
	for (size_t i = 0; i < pointLights.size() && i < 4; i++) {
		lightShader->Activate();

		// Set up matrices for light cube
		glm::mat4 lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, pointLights[i].position);
		lightModel = glm::scale(lightModel, glm::vec3(0.2f)); // Make them smaller

		// Set up view and projection matrices using the override camera
		glm::mat4 view = cameraOverride.GetViewMatrix();
		glm::mat4 projection = glm::perspective(
			glm::radians(cameraOverride.Zoom),
			//(float)WindowManager::GetWindowWidth() / (float)WindowManager::GetWindowHeight(),
			(float)RunTimeVar::window.width / (float)RunTimeVar::window.height,
			0.1f, 100.0f
		);

		lightShader->setMat4("model", lightModel);
		lightShader->setMat4("view", view);
		lightShader->setMat4("projection", projection);
		//lightShader->setVec3("lightColor", pointLights[i].diffuse); // Use light color

		lightCubeMesh->Draw(*lightShader, cameraOverride);
	}
}