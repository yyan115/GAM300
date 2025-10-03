#include "pch.h"
#include "Serialization/Serializer.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "ECS/ECSRegistry.hpp"
#include <ECS/NameComponent.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include "Hierarchy/EntityGUIDRegistry.hpp"

void Serializer::SerializeScene(const std::string& scenePath) {
    namespace fs = std::filesystem;

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
    auto& guidRegistry = EntityGUIDRegistry::GetInstance();

    // Iterate entities
    for (auto entity : ecs.GetAllEntities())
    {
        rapidjson::Value entObj(rapidjson::kObjectType);

        // add entity id (assumes entity is integer-like)
        {
            rapidjson::Value idv;
            idv.SetUint64(static_cast<uint64_t>(entity)); // adapt if entity type differs
            entObj.AddMember("id", idv, alloc);
            // convert GUID to string
            GUID_string entityGUIDStr =
                GUIDUtilities::ConvertGUID128ToString(
                    guidRegistry.GetGUIDByEntity(static_cast<Entity>(entity)));

            // create a RapidJSON string value with allocator
            rapidjson::Value guidv;
            guidv.SetString(entityGUIDStr.c_str(),
                static_cast<rapidjson::SizeType>(entityGUIDStr.length()),
                alloc);

            entObj.AddMember("guid", guidv, alloc);
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
        //if (ecs.HasComponent<DebugDrawComponent>(entity)) {
        //    auto& c = ecs.GetComponent<DebugDrawComponent>(entity);
        //    rapidjson::Value v = serializeComponentToValue(c);
        //    compsObj.AddMember("DebugDrawComponent", v, alloc);
        //}
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
        fs::path outPathP(scenePath);

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
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
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

void Serializer::DeserializeScene(const std::string& scenePath) {
    using namespace std;
    namespace fs = std::filesystem;

    fs::path pathToOpen = scenePath;
    if (!fs::exists(pathToOpen)) {
        // fallback to filename-only (in case save used only filename)
        pathToOpen = pathToOpen.filename();
    }

    if (!fs::exists(pathToOpen)) {
        std::cerr << "[CreateEntitiesFromJson] no scene JSON file found: " << scenePath << "\n";
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

        Entity newEnt{};
        uint64_t oldId = 0;
        if (entObj.HasMember("id") && entObj["id"].IsUint64()) oldId = entObj["id"].GetUint64();
        else if (entObj.HasMember("id") && entObj["id"].IsUint()) oldId = static_cast<uint64_t>(entObj["id"].GetUint());
        if (entObj.HasMember("guid")) {
            GUID_string guidStr = entObj["guid"].GetString();
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
            newEnt = ecs.CreateEntityWithGUID(guid);
        }
        else {
            // Fallback for if there is no GUID, but it shouldn't happen.
            newEnt = ecs.CreateEntity();
            ENGINE_LOG_WARN("Entity created with no GUID!");
        }

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

            if (mv.IsObject()) {
                if (mv.HasMember("data") && mv["data"].IsArray() && mv["data"].Size() > 0) {
                    const auto& arr = mv["data"];
                    ModelRenderComponent modelComp{};
                    GUID_string modelGUIDStr = mv["data"][0].GetString();
                    GUID_string shaderGUIDStr = mv["data"][1].GetString();
                    GUID_string materialGUIDStr = mv["data"][2].GetString();
                    modelComp.modelGUID = GUIDUtilities::ConvertStringToGUID128(modelGUIDStr);
                    modelComp.shaderGUID = GUIDUtilities::ConvertStringToGUID128(shaderGUIDStr);
                    modelComp.materialGUID = GUIDUtilities::ConvertStringToGUID128(materialGUIDStr);

                    ecs.AddComponent<ModelRenderComponent>(newEnt, modelComp);
                }
            }
        }

        // TextRenderComponent
        if (comps.HasMember("TextRenderComponent") && comps["TextRenderComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["TextRenderComponent"];

            // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
            if (tv.HasMember("data") && tv["data"].IsArray()) {
                TextRenderComponent textComp{};
                const auto& d = tv["data"];
                textComp.text = d[0]["data"].GetString();
                textComp.fontSize = d[1]["data"].GetUint();
                textComp.fontGUID = GUIDUtilities::ConvertStringToGUID128(d[2].GetString());
                textComp.shaderGUID = GUIDUtilities::ConvertStringToGUID128(d[3].GetString());
                readVec3Generic(d[4], textComp.position);
                readVec3Generic(d[5], textComp.color);
                textComp.scale = d[6]["data"].GetFloat();
                textComp.is3D = d[7]["data"].GetBool();
                textComp.alignment = static_cast<TextRenderComponent::Alignment>(d[9]["data"].GetInt());
                ecs.AddComponent<TextRenderComponent>(newEnt, textComp);
            }
        }

        // ParentComponent
        if (comps.HasMember("ParentComponent") && comps["ParentComponent"].IsObject()) {
            const auto& parentCompJSON = comps["ParentComponent"];
            ParentComponent parentComp{};
            GUID_string parentGUIDStr = parentCompJSON["data"][0].GetString();
            parentComp.parent = GUIDUtilities::ConvertStringToGUID128(parentGUIDStr);
            ecs.AddComponent(newEnt, parentComp);
        }

        // ChildrenComponent
        if (comps.HasMember("ChildrenComponent") && comps["ChildrenComponent"].IsObject()) {
            const auto& childrenCompJSON = comps["ChildrenComponent"];
            ChildrenComponent childrenComp{};
            if (childrenCompJSON.HasMember("data")) {
                const auto& childrenVectorJSON = childrenCompJSON["data"][0]["data"].GetArray();
                for (const auto& childJSON : childrenVectorJSON) {
                    GUID_string childGUIDStr = childJSON.GetString();
                    childrenComp.children.push_back(GUIDUtilities::ConvertStringToGUID128(childGUIDStr));
                }
            }

            ecs.AddComponent<ChildrenComponent>(newEnt, childrenComp);
        }

    } // end for entities

    std::cout << "[CreateEntitiesFromJson] loaded entities from: " << pathToOpen.string() << "\n";
}
