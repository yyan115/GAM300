#include "pch.h"
#include "Telemetry/TelemetrySystem.hpp"

#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/NameComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Animation/AnimationStateMachine.hpp"
#include "Animation/AnimationParam.hpp"
#include "Graphics/Camera/CameraSystem.hpp"
#include "Graphics/Camera/Camera.hpp"
#include "Script/ScriptComponentData.hpp"
#include "Scene/SceneManager.hpp"
#include "TimeManager.hpp"
#include "Scripting.h"
#include "Script/LuaBindableSystems.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

    bool g_initialised = false;
    bool g_enabled = false;
    std::string g_path;
    std::ofstream g_out;
    double g_accumulator = 0.0;
    unsigned long long g_frame = 0;

    // Hitstop is a dip of about 3 to 7 frames, and samples are 0.25s apart, so
    // reading the time scale at sample time would miss almost every dip. These
    // are updated every frame and reset when emitted, so a dip that happened
    // between two samples is still reported.
    float g_timeScaleMin = 1.0f;
    int g_timeScaleDips = 0;
    bool g_timeScaleWasDipped = false;

    // Peak of player_air_height since the last emit. A jump lasts about half a
    // second, so its apex falls between two 0.25s samples more often than not,
    // and reading the value at sample time understates how high the jump got.
    double g_airHeightPeak = 0.0;

    // ComboManager's state changes and early presses, watched every frame. A
    // light swing lasts under half a second, about two samples, so the state
    // read at sample time cannot say when a swing began or ended. Each change
    // is kept with its frame and the monotonic clock, which on Linux is the
    // clock another process reads with time.monotonic(), so input sent from
    // outside can be placed against the swing it landed in. A press made
    // before the combo window opens is queued, and queued_frames counts the
    // frames one was waiting.
    struct ComboEntry {
        std::string state;
        unsigned long long frame;
        double mono;
    };
    int g_comboRef = LUA_NOREF;
    std::string g_comboState;
    std::vector<ComboEntry> g_comboEntered;
    int g_comboQueuedFrames = 0;
    std::string g_lastScene;
    double g_walkmapPeriod = 0.0;   // seconds; 0 disables
    double g_walkmapAccum = 0.0;
    std::chrono::steady_clock::time_point g_start;

    // Sample rate. Fast enough to close a movement loop, slow enough that
    // the file stays readable over a long run. GAM300_TELEMETRY_INTERVAL
    // overrides it, down to every frame, for questions about what happened
    // on one frame, such as which animation an attacker was playing when the
    // player lost health.
    constexpr double kDefaultSampleInterval = 0.25;
    double g_sampleInterval = kDefaultSampleInterval;

    // Script basenames we care about. Entities are identified by which
    // script is attached rather than by name or tag, because enemies are
    // prefab instances whose names repeat across the level.
    constexpr const char* kPlayerScript = "PlayerHealth";
    constexpr const char* kEnemyScript = "EnemyAI";
    constexpr const char* kMinibossScript = "MinibossAI";
    constexpr const char* kInputScript = "InputInterpreter";
    constexpr const char* kChainScript = "ChainBootstrap";
    constexpr const char* kCameraScript = "camera_follow";
    constexpr const char* kFlythroughScript = "CameraFlythrough";
    constexpr const char* kComboScript = "ComboManager";

    std::string BaseName(const std::string& path) {
        const size_t slash = path.find_last_of("/\\");
        std::string file = (slash == std::string::npos) ? path : path.substr(slash + 1);
        const size_t dot = file.find_last_of('.');
        return (dot == std::string::npos) ? file : file.substr(0, dot);
    }

    // ── JSON emission ────────────────────────────────────────────────────
    // Hand-rolled because the engine has no JSON writer on this path and a
    // telemetry sink should not pull one in.

    void AppendEscaped(std::string& out, const std::string& s) {
        out += '"';
        for (const char c : s) {
            switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
            }
        }
        out += '"';
    }

    void AppendNumber(std::string& out, double v, int decimals = 3) {
        if (!std::isfinite(v)) { out += "null"; return; }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
        out += buf;
    }

    // ── Lua instance field reads ─────────────────────────────────────────
    // Every helper below leaves the Lua stack exactly as it found it. A
    // read-only observer that unbalances the stack would corrupt the very
    // run it is supposed to be measuring.

    bool PushInstance(lua_State* L, int instanceRef) {
        if (!L || instanceRef == LUA_NOREF || instanceRef == LUA_REFNIL) return false;
        lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    bool FieldNumber(lua_State* L, int instanceRef, const char* key, double& out) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, key);
        const bool ok = lua_isnumber(L, -1) != 0;
        if (ok) out = static_cast<double>(lua_tonumber(L, -1));
        lua_pop(L, 2);
        return ok;
    }

    // Length of a sequence field, and whether the field exists at all. The
    // flythrough builds `_waypoints` from its child entities and refuses to
    // run with fewer than two, so the count is the difference between "the
    // rig is wired up" and "the rig is a stub".
    bool FieldTableLen(lua_State* L, int instanceRef, const char* key, int& out) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, key);
        const bool ok = lua_istable(L, -1) != 0;
        if (ok) out = static_cast<int>(lua_rawlen(L, -1));
        lua_pop(L, 2);
        return ok;
    }

    bool FieldIsNil(lua_State* L, int instanceRef, const char* key, bool& isNil) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, key);
        isNil = lua_isnil(L, -1) != 0;
        lua_pop(L, 2);
        return true;
    }

    bool FieldBool(lua_State* L, int instanceRef, const char* key, bool& out) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, key);
        const bool present = !lua_isnil(L, -1);
        if (present) out = lua_toboolean(L, -1) != 0;
        lua_pop(L, 2);
        return present;
    }

    bool FieldString(lua_State* L, int instanceRef, const char* key, std::string& out) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, key);
        const bool ok = lua_isstring(L, -1) != 0;
        if (ok) out = lua_tostring(L, -1);
        lua_pop(L, 2);
        return ok;
    }

    // Reads instance[outer][inner] as a number, for controller.chainLen.
    bool FieldNestedNumber(lua_State* L, int instanceRef, const char* outer, const char* inner, double& out) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, outer);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return false;
        }
        lua_getfield(L, -1, inner);
        const bool ok = lua_isnumber(L, -1) != 0;
        if (ok) out = static_cast<double>(lua_tonumber(L, -1));
        lua_pop(L, 3);
        return ok;
    }

    // Reads instance[outer][inner] as a boolean, for controller.isExtending.
    bool FieldNestedBool(lua_State* L, int instanceRef, const char* outer, const char* inner, bool& out) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, outer);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return false;
        }
        lua_getfield(L, -1, inner);
        const bool present = !lua_isnil(L, -1);
        if (present) out = lua_toboolean(L, -1) != 0;
        lua_pop(L, 3);
        return present;
    }

    // Reads instance[outer][inner] as a three number array, for
    // controller.lockedEndPoint, which is where a shot came to rest.
    bool FieldNestedVec3(lua_State* L, int instanceRef, const char* outer, const char* inner, double out[3]) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, outer);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return false;
        }
        lua_getfield(L, -1, inner);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 3);
            return false;
        }
        bool ok = true;
        for (int i = 0; i < 3; ++i) {
            lua_rawgeti(L, -1, i + 1);
            if (lua_isnumber(L, -1)) {
                out[i] = static_cast<double>(lua_tonumber(L, -1));
            } else {
                ok = false;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 3);
        return ok;
    }

    // Reads instance[key] as an {x,y,z} table, for ChainBootstrap's
    // _cameraAimWorldPoint, which is the point the chain is actually aimed at.
    bool FieldXYZ(lua_State* L, int instanceRef, const char* key, double out[3]) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, key);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return false;
        }
        const char* names[3] = {"x", "y", "z"};
        bool ok = true;
        for (int i = 0; i < 3; ++i) {
            lua_getfield(L, -1, names[i]);
            if (lua_isnumber(L, -1)) {
                out[i] = static_cast<double>(lua_tonumber(L, -1));
            } else {
                ok = false;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 2);
        return ok;
    }

    // Guards the crosshair cast: a zero or non-finite forward is meaningless
    // and Jolt will not thank us for it.
    bool phys_ok(double x, double y, double z) {
        const double n = x * x + y * y + z * z;
        return std::isfinite(n) && n > 1e-6;
    }

    // Reads instance[table][key].field where key is a number, for
    // camera_follow's _lockonHitCounts, which is keyed by entity id and holds
    // { count, firstHitAt } per enemy. Needed to tell a lock-on switch that
    // the policy allows from one it does not: a switch is legitimate either
    // after the engaged enemy goes quiet, or once another has been hit twice
    // inside the window, and without the count those two look identical.
    bool FieldTableEntryNumber(lua_State* L, int instanceRef, const char* table,
                               double key, const char* field, double& out) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, table);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return false;
        }
        lua_pushnumber(L, key);
        lua_gettable(L, -2);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 3);
            return false;
        }
        lua_getfield(L, -1, field);
        const bool ok = lua_isnumber(L, -1) != 0;
        if (ok) out = static_cast<double>(lua_tonumber(L, -1));
        lua_pop(L, 4);
        return ok;
    }

    bool GlobalNumber(lua_State* L, const char* name, double& out) {
        if (!L) return false;
        lua_getglobal(L, name);
        const bool ok = lua_isnumber(L, -1) != 0;
        if (ok) out = static_cast<double>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        return ok;
    }

    // One string field inside a table field on the instance: instance[key][sub].
    bool FieldTableEntryString(lua_State* L, int instanceRef, const char* key,
                               const char* sub, std::string& out) {
        if (!L || instanceRef == LUA_NOREF) return false;
        lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef);
        if (!lua_istable(L, -1)) { lua_pop(L, 1); return false; }
        lua_getfield(L, -1, key);
        if (!lua_istable(L, -1)) { lua_pop(L, 2); return false; }
        lua_getfield(L, -1, sub);
        const bool ok = lua_isstring(L, -1) != 0;
        if (ok) out = lua_tostring(L, -1);
        lua_pop(L, 3);
        return ok;
    }

    bool GlobalString(lua_State* L, const char* name, std::string& out) {
        if (!L) return false;
        lua_getglobal(L, name);
        const bool ok = lua_isstring(L, -1) != 0;
        if (ok) out = lua_tostring(L, -1);
        lua_pop(L, 1);
        return ok;
    }

    bool GlobalBool(lua_State* L, const char* name, bool& out) {
        if (!L) return false;
        lua_getglobal(L, name);
        const bool present = !lua_isnil(L, -1);
        if (present) out = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return present;
    }

    // Reads instance[outer][inner] as a string, for fsm.currentName.
    bool FieldNestedString(lua_State* L, int instanceRef, const char* outer, const char* inner, std::string& out) {
        if (!PushInstance(L, instanceRef)) return false;
        lua_getfield(L, -1, outer);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return false;
        }
        lua_getfield(L, -1, inner);
        const bool ok = lua_isstring(L, -1) != 0;
        if (ok) out = lua_tostring(L, -1);
        lua_pop(L, 3);
        return ok;
    }

    // Mirrors EnemyAI.lua's IsFlying(): strip quotes and whitespace, fold
    // case, compare. One enemy in 04_Level has its EnemyType authored as the
    // literal string "\"Flying\"", quote characters included. The game copes
    // because IsFlying normalises, but a consumer comparing the raw string
    // classifies that enemy as a ground unit. A static scan made exactly that
    // mistake and concluded the statue room had one flying enemy when it has
    // two, so normalise here rather than leave the trap in place downstream.
    bool IsFlyingType(const std::string& raw) {
        std::string t;
        t.reserve(raw.size());
        for (const char c : raw) {
            if (c == '"' || std::isspace(static_cast<unsigned char>(c))) continue;
            t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return t == "flying";
    }

    // camera_follow.lua publishes its orbit yaw as the global CAMERA_YAW.
    // WASD is camera-relative, so without this a world-space destination
    // cannot be turned into a key to hold: the same key walks a different
    // direction depending on where the camera happens to be pointing.
    bool CameraYaw(lua_State* L, double& out) {
        if (!L) return false;
        lua_getglobal(L, "CAMERA_YAW");
        const bool ok = lua_isnumber(L, -1) != 0;
        if (ok) out = static_cast<double>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        return ok;
    }

    // Yaw about Y, in degrees, from the world rotation quaternion.
    double YawDegrees(const Quaternion& q) {
        const double siny_cosp = 2.0 * (static_cast<double>(q.w) * q.y + static_cast<double>(q.x) * q.z);
        const double cosy_cosp = 1.0 - 2.0 * (static_cast<double>(q.y) * q.y + static_cast<double>(q.z) * q.z);
        return std::atan2(siny_cosp, cosy_cosp) * 180.0 / 3.14159265358979323846;
    }

    // A census is written once per scene load: every named entity with a
    // transform, in WORLD coordinates.
    //
    // This exists because the scene file cannot answer the question. Transform
    // is serialised as localPosition/localScale/localRotation only (see
    // REFL_REGISTER_START(Transform)), so every position on disk is relative
    // to a parent, and rooms nest their contents several levels deep. Reading
    // a door or pickup's world position statically means recomposing the whole
    // ancestor chain. The running game has already done that work, so ask it.
    void WriteCensus(ECSManager& ecs, const std::string& scene) {
        std::string line;
        line.reserve(1 << 16);
        line += "{\"type\":\"census\",\"scene\":";
        AppendEscaped(line, scene);
        line += ",\"entities\":[";

        bool first = true;
        for (const Entity entity : ecs.GetAllEntities()) {
            auto nameOpt = ecs.TryGetComponent<NameComponent>(entity);
            if (!nameOpt.has_value()) continue;
            const std::string& name = nameOpt.value().get().name;
            if (name.empty()) continue;

            auto transformOpt = ecs.TryGetComponent<Transform>(entity);
            if (!transformOpt.has_value()) continue;
            const Transform& tr = transformOpt.value().get();

            if (!first) line += ",";
            first = false;
            line += "{\"id\":";
            line += std::to_string(entity);
            line += ",\"name\":";
            AppendEscaped(line, name);
            line += ",\"x\":"; AppendNumber(line, tr.worldPosition.x);
            line += ",\"y\":"; AppendNumber(line, tr.worldPosition.y);
            line += ",\"z\":"; AppendNumber(line, tr.worldPosition.z);
            line += "}";
        }
        line += "]}\n";
        g_out << line;
        g_out.flush();
    }

    // A local height map, sampled by casting rays straight down on a grid
    // around the player.
    //
    // This exists because neither the scene file nor the engine's own nav
    // data can say where the player may walk. NavGrid is a single XZ layer
    // bounded to +/-20 with one ground height per cell, so it cannot
    // represent a balcony above a floor and does not reach the statue room
    // at x 23 to 42 at all. Without geometry, an external driver navigates by
    // walking into walls and backing off, which is slow and cannot tell a
    // staircase from a bookshelf.
    //
    // A downward ray reports the top of whatever occupies a cell, so a wall
    // reads as a tall column and a step reads as a small rise. That is enough
    // to route: a neighbouring cell is reachable when its height differs from
    // the current one by less than the character's step height.
    void WriteWalkmap(const Vector3D& centre) {
        PhysicsSystem* phys = PhysicsSystemWrappers::g_PhysicsSystem;
        if (!phys) return;

        constexpr int kHalf = 24;          // cells each side of the player
        constexpr float kCell = 0.5f;      // world units per cell
        // Start just above head height, not higher. Storeys in this level are
        // about 3.27 apart, so a ray starting several units up begins above
        // the ceiling and reports the floor of the storey above instead of
        // the one the player is standing on.
        constexpr float kUp = 1.5f;
        constexpr float kDown = 8.0f;

        std::string line;
        line.reserve(1 << 15);
        line += "{\"type\":\"walkmap\",\"cx\":"; AppendNumber(line, centre.x);
        line += ",\"cz\":"; AppendNumber(line, centre.z);
        line += ",\"cy\":"; AppendNumber(line, centre.y);
        line += ",\"cell\":"; AppendNumber(line, kCell, 2);
        line += ",\"half\":"; line += std::to_string(kHalf);
        line += ",\"h\":[";

        const Vector3D down(0.0f, -1.0f, 0.0f);
        bool first = true;
        for (int gz = -kHalf; gz <= kHalf; ++gz) {
            for (int gx = -kHalf; gx <= kHalf; ++gx) {
                const float wx = centre.x + gx * kCell;
                const float wz = centre.z + gz * kCell;
                const Vector3D origin(wx, centre.y + kUp, wz);
                const auto r = phys->Raycast(origin, down, kDown);
                if (!first) line += ",";
                first = false;
                if (r.hit) {
                    AppendNumber(line, centre.y + kUp - r.distance, 2);
                } else {
                    line += "null";
                }
            }
        }
        line += "]}\n";
        g_out << line;
        g_out.flush();
    }

    struct Actor {
        Entity entity = 0;
        std::string name;
        Vector3D pos{};
        double yaw = 0.0;
        int instanceRef = LUA_NOREF;
        // The animation state machine's current state, and which clip is
        // actually playing. Reported alongside the AI's own FSM state so the
        // two can be compared: "the enemy is idle but playing the attack
        // clip" is a claim about the relationship between them, and without
        // both it can only be judged by eye from a screenshot.
        bool haveAnim = false;
        std::string animState;
        long long animClip = -1;
        // Which animation FILE each clip slot actually resolves to.
        //
        // The state name and the clip index are both the state machine's own
        // account of itself, so comparing them can only ever agree. Neither
        // says which asset was loaded into that slot. A clip slot resolved by
        // a stale GUID loads a different file while still being called
        // "Idle" and still reporting index 2, so an enemy can stand in Idle,
        // report Idle, and play the attack animation with nothing in either
        // field disagreeing. This is the only field that can see that.
        std::string animAsset;
        std::string animMiswired;
        // The animator's own parameters. The transitions out of an attack
        // state are gated on these, so when an enemy is stuck in one, the
        // parameter that is holding it there is the answer and everything
        // else is inference.
        std::string animParams;
    };

    // The loader resolves a clip by GUID first and only falls back to the
    // authored path, so this repeats that decision rather than assuming the
    // two agree.
    static std::string ResolvedClipFile(const AnimationComponent& anim, size_t i) {
        if (i < anim.clipGUIDs.size()) {
            const GUID_128 guid = anim.clipGUIDs[i];
            if (guid.high != 0 || guid.low != 0) {
                // GetAssetPathFromGUID logs an error for every GUID it cannot
                // resolve, and this runs for every clip of every actor on every
                // sample. Twenty unresolvable clip GUIDs in the level turned
                // into 2339 error lines in a single run, which buried the rest
                // of the log. The answer for one GUID never changes within a
                // run, so ask once and remember it.
                static std::unordered_map<GUID_128, std::string> cache;
                auto it = cache.find(guid);
                if (it == cache.end()) {
                    it = cache.emplace(
                        guid,
                        AssetManager::GetInstance().GetAssetPathFromGUID(guid)).first;
                }
                if (!it->second.empty()) return BaseName(it->second);
            }
        }
        if (i < anim.clipPaths.size()) return BaseName(anim.clipPaths[i]);
        return {};
    }

    void AppendActorCommon(std::string& out, const Actor& a) {
        out += "{\"id\":";
        out += std::to_string(a.entity);
        out += ",\"name\":";
        AppendEscaped(out, a.name);
        out += ",\"x\":"; AppendNumber(out, a.pos.x);
        out += ",\"y\":"; AppendNumber(out, a.pos.y);
        out += ",\"z\":"; AppendNumber(out, a.pos.z);
        out += ",\"yaw\":"; AppendNumber(out, a.yaw, 1);
    }

}  // namespace

namespace Telemetry {

    bool IsEnabled() { return g_enabled; }

    const std::string& OutputPath() { return g_path; }

    void Initialise() {
        if (g_initialised) return;
        g_initialised = true;

        const char* flag = std::getenv("GAM300_TELEMETRY");
        if (!flag || std::string(flag) != "1") {
            g_enabled = false;
            return;
        }

        // Optional, and off by default: sampling a 49x49 grid is 2401 raycasts,
        // which is cheap at a fraction of a hertz and wasteful every frame.
        if (const char* wm = std::getenv("GAM300_TELEMETRY_WALKMAP")) {
            g_walkmapPeriod = std::atof(wm);
            if (g_walkmapPeriod < 0.0) g_walkmapPeriod = 0.0;
        }

        if (const char* iv = std::getenv("GAM300_TELEMETRY_INTERVAL")) {
            g_sampleInterval = std::atof(iv);
            if (g_sampleInterval < 0.0) g_sampleInterval = kDefaultSampleInterval;
        }

        const char* path = std::getenv("GAM300_TELEMETRY_PATH");
        g_path = (path && *path) ? path : "telemetry.jsonl";

        g_out.open(g_path, std::ios::out | std::ios::trunc);
        if (!g_out.is_open()) {
            std::fprintf(stderr, "[telemetry] cannot open '%s' for writing; disabled\n", g_path.c_str());
            g_enabled = false;
            return;
        }

        g_enabled = true;
        g_start = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[telemetry] enabled, writing to %s\n", g_path.c_str());
    }

    void Shutdown() {
        if (g_out.is_open()) {
            g_out.flush();
            g_out.close();
        }
        g_enabled = false;
    }

    void Sample() {
        if (!g_enabled) return;

        ++g_frame;
        {
            const float ts = TimeManager::GetTimeScale();
            if (ts < g_timeScaleMin) g_timeScaleMin = ts;
            const bool dipped = ts < 0.999f;
            if (dipped && !g_timeScaleWasDipped) ++g_timeScaleDips;
            g_timeScaleWasDipped = dipped;
        }
        {
            lua_State* Lp = Scripting::GetLuaState();
            double ah = 0.0;
            if (Lp && GlobalNumber(Lp, "player_air_height", ah) && ah > g_airHeightPeak) {
                g_airHeightPeak = ah;
            }
            std::string comboState;
            if (Lp && FieldString(Lp, g_comboRef, "_currentStateId", comboState)) {
                if (comboState != g_comboState) {
                    g_comboState = comboState;
                    const double mono = std::chrono::duration<double>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    g_comboEntered.push_back({comboState, g_frame, mono});
                }
                bool queuedIsNil = true;
                if (FieldIsNil(Lp, g_comboRef, "_queuedCombo", queuedIsNil) && !queuedIsNil) {
                    ++g_comboQueuedFrames;
                }
            }
        }
        g_accumulator += TimeManager::GetUnscaledDeltaTime();
        if (g_accumulator < g_sampleInterval) return;
        const double sinceLastSample = g_accumulator;
        g_accumulator = 0.0;

        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        lua_State* L = Scripting::GetLuaState();

        const std::string scene = SceneManager::GetInstance().GetSceneName();
        if (scene != g_lastScene) {
            g_lastScene = scene;
            WriteCensus(ecs, scene);
        }

        Actor player;
        bool havePlayer = false;
        int inputRef = LUA_NOREF;
        bool haveInput = false;
        int chainRef = LUA_NOREF;
        bool haveChain = false;
        int cameraRef = LUA_NOREF;
        int comboRef = LUA_NOREF;
        // Every CameraFlythrough rig in the scene, so a stub can be told from a
        // working one without reading Lua print output, which never reaches the log.
        std::vector<std::pair<Entity, int>> flyRefs;
        bool haveCamera = false;
        std::vector<std::pair<Actor, const char*>> enemies;  // actor, script kind

        for (const Entity entity : ecs.GetAllEntities()) {
            auto scriptOpt = ecs.TryGetComponent<ScriptComponentData>(entity);
            if (!scriptOpt.has_value()) continue;

            const ScriptComponentData& sc = scriptOpt.value().get();
            const char* kind = nullptr;
            int instanceRef = LUA_NOREF;

            // One entity carries several of these scripts. The Player holds
            // PlayerHealth and ComboManager together, so a loop that stopped
            // at the first script it recognised reported whichever came first
            // and never saw the other: the combo block came back empty on a
            // scene that has a ComboManager. So the whole list is scanned, the
            // scripts that are not actors are taken wherever they appear, and
            // only the actor kind is settled once.
            for (const ScriptData& sd : sc.scripts) {
                if (!sd.enabled) continue;
                const std::string base = BaseName(sd.scriptPath);
                const int ref = sd.instanceCreated ? sd.instanceId : LUA_NOREF;

                if (base == kInputScript)       { inputRef = ref;  haveInput = true;  continue; }
                if (base == kChainScript)       { chainRef = ref;  haveChain = true;  continue; }
                if (base == kCameraScript)      { cameraRef = ref; haveCamera = true; continue; }
                if (base == kComboScript)       { comboRef = ref;  continue; }
                if (base == kFlythroughScript)  { flyRefs.push_back({entity, ref});   continue; }

                if (kind) continue;  // actor kind already settled for this entity
                if (base == kPlayerScript)        kind = kPlayerScript;
                else if (base == kEnemyScript)    kind = kEnemyScript;
                else if (base == kMinibossScript) kind = kMinibossScript;
                else continue;
                instanceRef = ref;
            }
            if (!kind) continue;


            auto transformOpt = ecs.TryGetComponent<Transform>(entity);
            if (!transformOpt.has_value()) continue;
            const Transform& tr = transformOpt.value().get();

            Actor a;
            a.entity = entity;
            a.pos = tr.worldPosition;
            a.yaw = YawDegrees(tr.worldRotation);
            a.instanceRef = instanceRef;
            if (auto animOpt = ecs.TryGetComponent<AnimationComponent>(entity); animOpt.has_value()) {
                AnimationComponent& anim = animOpt.value().get();
                a.haveAnim = true;
                a.animState = anim.GetCurrentState();
                const size_t active = anim.GetActiveClipIndex();
                a.animClip = static_cast<long long>(active);
                a.animAsset = ResolvedClipFile(anim, active);
                if (const AnimationStateMachine* sm = anim.GetStateMachine()) {
                    for (const auto& kv : sm->GetParams().GetAllParams()) {
                        double v = 0.0;
                        switch (kv.second.type) {
                            case AnimParamType::Bool:
                            case AnimParamType::Trigger:
                                v = std::get<bool>(kv.second.value) ? 1.0 : 0.0;
                                break;
                            case AnimParamType::Int:
                                v = static_cast<double>(std::get<int>(kv.second.value));
                                break;
                            case AnimParamType::Float:
                                v = static_cast<double>(std::get<float>(kv.second.value));
                                break;
                        }
                        if (v == 0.0) continue;          // only the ones that are set
                        if (!a.animParams.empty()) a.animParams += " ";
                        a.animParams += kv.first;
                    }
                }
                const size_t slots =
                    anim.clipPaths.size() > anim.clipGUIDs.size()
                        ? anim.clipPaths.size() : anim.clipGUIDs.size();
                for (size_t i = 0; i < slots; ++i) {
                    if (i >= anim.clipPaths.size()) continue;
                    const std::string authored = BaseName(anim.clipPaths[i]);
                    const std::string got = ResolvedClipFile(anim, i);
                    if (authored.empty() || got.empty() || authored == got) continue;
                    if (!a.animMiswired.empty()) a.animMiswired += " ";
                    a.animMiswired += std::to_string(i) + ":" + authored + ">" + got;
                }
            }
            if (auto nameOpt = ecs.TryGetComponent<NameComponent>(entity); nameOpt.has_value()) {
                a.name = nameOpt.value().get().name;
            }

            if (kind == kPlayerScript) {
                player = a;
                havePlayer = true;
            } else {
                enemies.emplace_back(a, kind);
            }
        }

        // Found again on every sample, so a scene change that replaces the
        // player is picked up within one sample.
        g_comboRef = comboRef;

        std::string line;
        line.reserve(2048);

        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();

        line += "{\"t\":"; AppendNumber(line, elapsed, 2);
        line += ",\"frame\":"; line += std::to_string(g_frame);
        line += ",\"scene\":";
        AppendEscaped(line, scene);

        double camYaw = 0.0;
        if (CameraYaw(L, camYaw)) {
            line += ",\"camera_yaw\":";
            AppendNumber(line, camYaw, 2);
        }
        double camPitch = 0.0;
        if (GlobalNumber(L, "CAMERA_PITCH", camPitch)) {
            line += ",\"camera_pitch\":";
            AppendNumber(line, camPitch, 2);
        }
        // The camera's world position and unit forward vector, which
        // camera_follow.lua already publishes. The chain fires along this
        // forward, so having it turns aiming from guess-and-check into a
        // measurable angular error against the direction to the target.
        double fx = 0.0, fy = 0.0, fz = 0.0;
        if (GlobalNumber(L, "CAMERA_FWD_X", fx)
            && GlobalNumber(L, "CAMERA_FWD_Y", fy)
            && GlobalNumber(L, "CAMERA_FWD_Z", fz)) {
            line += ",\"camera_fwd\":[";
            AppendNumber(line, fx, 4); line += ",";
            AppendNumber(line, fy, 4); line += ",";
            AppendNumber(line, fz, 4);
            line += "]";
        }
        // Prefer the real render camera over the Lua globals.
        //
        // camera_follow.lua publishes CAMERA_POS_* every frame, but
        // `Cinematic.updateCinematic` returns early before that code runs, so
        // during a flythrough the globals freeze at the last gameplay value
        // while the view is actually moving. That made a working flythrough
        // look like a broken one for half an hour. The camera system knows the
        // truth; the globals are the fallback.
        double px = 0.0, py = 0.0, pz = 0.0;
        bool havePos = false;
        bool camFromEngine = false;
        if (const Camera* cam = ecs.cameraSystem ? ecs.cameraSystem->GetActiveCamera() : nullptr) {
            px = cam->Position.x; py = cam->Position.y; pz = cam->Position.z;
            havePos = true;
            camFromEngine = true;
            line += ",\"camera_fwd_true\":[";
            AppendNumber(line, cam->Front.x, 4); line += ",";
            AppendNumber(line, cam->Front.y, 4); line += ",";
            AppendNumber(line, cam->Front.z, 4);
            line += "]";
        }
        if (!havePos) {
            havePos = GlobalNumber(L, "CAMERA_POS_X", px)
                      && GlobalNumber(L, "CAMERA_POS_Y", py)
                      && GlobalNumber(L, "CAMERA_POS_Z", pz);
        }
        (void)camFromEngine;
        if (havePos) {
            line += ",\"camera_pos\":[";
            AppendNumber(line, px); line += ",";
            AppendNumber(line, py); line += ",";
            AppendNumber(line, pz);
            line += "]";
        }

        // What the crosshair is actually on.
        //
        // This is the quantity that decides whether a hook connects, and it
        // is not the same as where the camera is pointed. camera_chain_aim.lua
        // raycasts from the camera along its forward, and publishes the hit
        // point as the chain's world target; ChainBootstrap then fires from
        // the player's HAND toward that point. So a shot lands on an enemy
        // only when the camera ray strikes the enemy's collider. A ray that
        // passes a few centimetres over it takes its world target from the
        // wall eight units behind, and the chain flies to that wall instead,
        // which is exactly what a dozen failed hooks on the second statue
        // room flyer were doing.
        if (havePos && phys_ok(fx, fy, fz)) {
            PhysicsSystem* phys = PhysicsSystemWrappers::g_PhysicsSystem;
            if (phys) {
                const auto hit = phys->Raycast(Vector3D(static_cast<float>(px),
                                                        static_cast<float>(py),
                                                        static_cast<float>(pz)),
                                               Vector3D(static_cast<float>(fx),
                                                        static_cast<float>(fy),
                                                        static_cast<float>(fz)),
                                               100.0f);
                line += ",\"crosshair\":";
                if (hit.hit) {
                    line += "{\"dist\":"; AppendNumber(line, hit.distance);
                    line += ",\"x\":"; AppendNumber(line, hit.hitPoint.x);
                    line += ",\"y\":"; AppendNumber(line, hit.hitPoint.y);
                    line += ",\"z\":"; AppendNumber(line, hit.hitPoint.z);
                    line += ",\"entity\":"; line += std::to_string(hit.entityId);
                    line += "}";
                } else {
                    line += "null";
                }
            }
        }

        line += ",\"player\":";
        if (havePlayer) {
            AppendActorCommon(line, player);
            double hp = 0.0, maxHp = 0.0;
            bool god = false;
            if (FieldNumber(L, player.instanceRef, "CurrentHealth", hp)) {
                line += ",\"hp\":"; AppendNumber(line, hp, 1);
            }
            if (FieldNumber(L, player.instanceRef, "MaxHealth", maxHp)) {
                line += ",\"max_hp\":"; AppendNumber(line, maxHp, 1);
            }
            if (FieldBool(L, player.instanceRef, "GodMode", god)) {
                line += ",\"godmode\":";
                line += god ? "true" : "false";
            }
            // Feather count and the skill's cost, both Lua globals. The
            // feather skill's only externally visible effect is this number
            // going down, so without it there is no way to tell a cast that
            // fired and did nothing from a keypress the game ignored.
            double feathers = 0.0;
            if (GlobalNumber(L, "_numFeathers", feathers)) {
                line += ",\"feathers\":"; AppendNumber(line, feathers, 0);
            }
            double featherCost = 0.0;
            if (GlobalNumber(L, "_featherSkillRequirement", featherCost)) {
                line += ",\"feather_skill_cost\":"; AppendNumber(line, featherCost, 0);
            }
            line += "}";
        } else {
            line += "null";
        }

        // Whether the game is currently ignoring input matters as much as
        // where the player is. A cinematic freeze looks identical to walking
        // into a wall from the outside: position stops changing while keys
        // are held. Telling them apart from telemetry stops a driver from
        // reporting "stuck" when the right response is "wait".
        if (haveInput) {
            bool frozen = false, dead = false;
            const bool haveFrozen = FieldBool(L, inputRef, "_frozenByCinematic", frozen);
            const bool haveDead = FieldBool(L, inputRef, "_playerDead", dead);
            if (haveFrozen || haveDead) {
                line += ",\"input\":{\"frozen\":";
                line += (haveFrozen && frozen) ? "true" : "false";
                line += ",\"dead\":";
                line += (haveDead && dead) ? "true" : "false";
                line += "}";
            }
        }

        // Chain state. Without it the hook is opaque: a shot that never fired,
        // one that fired into a wall, and one that hit an enemy all look
        // identical from outside, because the only visible consequence of a
        // successful hook is a change on the enemy that a miss does not
        // produce either.
        if (haveChain) {
            bool aiming = false;
            const bool haveAim = GlobalBool(L, "CHAIN_AIM_ACTIVE", aiming);
            double len = 0.0;
            const bool haveLen = FieldNestedNumber(L, chainRef, "controller", "chainLen", len);
            bool extending = false, locked = false, snapped = false;
            const bool haveExt = FieldNestedBool(L, chainRef, "controller", "isExtending", extending);
            FieldNestedBool(L, chainRef, "controller", "endPointLocked", locked);
            FieldNestedBool(L, chainRef, "controller", "_raycastSnapped", snapped);
            if (haveAim || haveLen || haveExt) {
                line += ",\"chain\":{\"aiming\":";
                line += aiming ? "true" : "false";
                line += ",\"len\":"; AppendNumber(line, len, 2);
                line += ",\"extending\":";
                line += extending ? "true" : "false";
                line += ",\"locked\":";
                line += locked ? "true" : "false";
                line += ",\"wall\":";
                line += snapped ? "true" : "false";

                // What the chain believes it hooked. A shot can report locked
                // while the enemy carries on attacking, measured at 0 of 6
                // hooks producing a Hooked state, and from outside there is no
                // way to tell whether the hook event was never published or
                // was published at an id no enemy matches. hooked_enemy is the
                // id ChainBootstrap will publish, and hooked_tag is the tag
                // that decides whether it sets that id at all.
                double hookedEnemy = 0.0;
                if (FieldNumber(L, chainRef, "_hookedEnemyEntityId", hookedEnemy)) {
                    line += ",\"hooked_enemy\":";
                    line += std::to_string(static_cast<long long>(hookedEnemy));
                } else {
                    line += ",\"hooked_enemy\":null";
                }
                std::string hookedTag;
                if (FieldNestedString(L, chainRef, "controller", "hookedTag", hookedTag)) {
                    line += ",\"hooked_tag\":"; AppendEscaped(line, hookedTag);
                }

                // Where the shot came to rest. Without this there is no way to
                // check from outside whether a throw went where the crosshair
                // was pointing when the button came up.
                double endPoint[3] = {0.0, 0.0, 0.0};
                if (FieldNestedVec3(L, chainRef, "controller", "lockedEndPoint", endPoint)) {
                    line += ",\"end\":[";
                    AppendNumber(line, endPoint[0], 3); line += ",";
                    AppendNumber(line, endPoint[1], 3); line += ",";
                    AppendNumber(line, endPoint[2], 3); line += "]";
                }

                // Where the chain leaves the player, which is the hand bone and
                // not the camera. Asking whether a throw went where the player
                // was looking is a question about the angle at this point, and
                // measuring it at the camera instead reads the metre between
                // the two as aim error.
                double startPoint[3] = {0.0, 0.0, 0.0};
                if (FieldNestedVec3(L, chainRef, "controller", "startPos", startPoint)) {
                    line += ",\"start\":[";
                    AppendNumber(line, startPoint[0], 3); line += ",";
                    AppendNumber(line, startPoint[1], 3); line += ",";
                    AppendNumber(line, startPoint[2], 3); line += "]";
                }

                // The point the chain is aimed at, as the game has it rather
                // than as an outside raycast reconstructs it. The crosshair
                // field above casts from camera_pos, and during a chain aim the
                // camera sits at an over the shoulder offset that field does
                // not know about, so the two are not the same point and only
                // this one decides where a throw goes.
                double aimPoint[3] = {0.0, 0.0, 0.0};
                if (FieldXYZ(L, chainRef, "_cameraAimWorldPoint", aimPoint)) {
                    line += ",\"aim\":[";
                    AppendNumber(line, aimPoint[0], 3); line += ",";
                    AppendNumber(line, aimPoint[1], 3); line += ",";
                    AppendNumber(line, aimPoint[2], 3); line += "]";
                }
                double heldAim[3] = {0.0, 0.0, 0.0};
                if (FieldXYZ(L, chainRef, "_releaseAimWorldPoint", heldAim)) {
                    line += ",\"aim_held\":[";
                    AppendNumber(line, heldAim[0], 3); line += ",";
                    AppendNumber(line, heldAim[1], 3); line += ",";
                    AppendNumber(line, heldAim[2], 3); line += "]";
                }
                line += "}";
            }
        }

        // Which enemy the camera has locked onto, if any. Two open reports
        // are about this: the camera swinging between enemies mid-fight, and
        // the camera locking onto nothing or onto a corpse. Both are claims
        // about how this value changes over time, and neither can be checked
        // from outside without it. Reported next to every enemy's dead flag,
        // so "locked onto an enemy that is already dead" is a comparison
        // rather than an impression.
        if (!flyRefs.empty()) {
            line += ",\"flythrough\":[";
            bool firstFly = true;
            for (const auto& fr : flyRefs) {
                if (!firstFly) line += ",";
                firstFly = false;
                line += "{\"id\":";
                line += std::to_string(fr.first);
                bool active = false;
                if (FieldBool(L, fr.second, "_active", active)) {
                    line += ",\"active\":";
                    line += active ? "true" : "false";
                }
                int wps = 0;
                if (FieldTableLen(L, fr.second, "_waypoints", wps)) {
                    line += ",\"waypoints\":";
                    line += std::to_string(wps);
                } else {
                    line += ",\"waypoints\":null";
                }
                double timer = 0.0;
                if (FieldNumber(L, fr.second, "_timer", timer)) {
                    line += ",\"timer\":"; AppendNumber(line, timer, 2);
                }
                double fkey = 0.0;
                if (FieldNumber(L, fr.second, "FKey", fkey)) {
                    line += ",\"fkey\":";
                    line += std::to_string(static_cast<long long>(fkey));
                }
                double dur = 0.0;
                if (FieldNumber(L, fr.second, "TotalDuration", dur)) {
                    line += ",\"duration\":"; AppendNumber(line, dur, 2);
                }
                line += "}";
            }
            line += "]";
        }

        if (haveCamera) {
            bool cineActive = false;
            if (FieldBool(L, cameraRef, "_cinematicActive", cineActive)) {
                line += ",\"cinematic_active\":";
                line += cineActive ? "true" : "false";
            }
            bool tgtNil = true;
            if (FieldIsNil(L, cameraRef, "_cinematicTarget", tgtNil)) {
                line += ",\"cinematic_target\":";
                line += tgtNil ? "false" : "true";
            }
            bool lockActive = false;
            const bool haveActive = FieldBool(L, cameraRef, "_lockonActive", lockActive);
            double lockEntity = -1.0;
            const bool haveEntity = FieldNumber(L, cameraRef, "_lockonEntityId", lockEntity);
            if (haveActive || haveEntity) {
                line += ",\"lockon\":{\"active\":";
                line += lockActive ? "true" : "false";
                line += ",\"entity\":";
                if (haveEntity) line += std::to_string(static_cast<long long>(lockEntity));
                else line += "null";

                // The clock, when the engaged enemy was last hit, and how many
                // hits the locked enemy has accumulated. Together these say
                // whether a switch took the quiet-delay path or the
                // repeat-hit path, which is the difference between the camera
                // being stolen and the player deliberately changing target.
                double clock = 0.0, engagedAt = 0.0;
                if (FieldNumber(L, cameraRef, "_lockonClock", clock)) {
                    line += ",\"clock\":"; AppendNumber(line, clock, 2);
                }
                if (FieldNumber(L, cameraRef, "_lockonEngagedAt", engagedAt)) {
                    line += ",\"engaged_at\":"; AppendNumber(line, engagedAt, 2);
                }
                double hits = 0.0;
                if (haveEntity && lockEntity >= 0
                    && FieldTableEntryNumber(L, cameraRef, "_lockonHitCounts",
                                             lockEntity, "count", hits)) {
                    line += ",\"hits\":"; AppendNumber(line, hits, 0);
                }
                line += "}";
            }
        }

        // Time scale over the interval just ended, not at the instant of the
        // sample. min is the deepest dip; dips counts how many separate dips
        // began. Hitstop used to fire only for damage at or above 20, which is
        // four of the nine attacks in the combo tree, so this is what says
        // whether a light hit now produces a pause at all.
        line += ",\"time_scale\":{\"now\":";
        AppendNumber(line, TimeManager::GetTimeScale(), 3);
        line += ",\"min\":"; AppendNumber(line, g_timeScaleMin, 3);
        line += ",\"dips\":"; line += std::to_string(g_timeScaleDips);
        line += "}";
        g_timeScaleMin = TimeManager::GetTimeScale();
        g_timeScaleDips = 0;

        // The combo state machine. air_light_2 branches on whether the aerial
        // hit confirmed: confirmed loops back to air_light_1 and stays
        // airborne, unconfirmed commits to air_slam. Nothing published the
        // confirm event, so the flag was permanently false and the loop had
        // never once happened. aerial_hit is that flag.
        if (comboRef != LUA_NOREF) {
            std::string stateId;
            bool aerialHit = false, canMove = false;
            double stateTimer = 0.0;
            line += ",\"combo\":{";
            bool wrote = false;
            if (FieldString(L, comboRef, "_currentStateId", stateId)) {
                line += "\"state\":"; AppendEscaped(line, stateId); wrote = true;
            }
            if (FieldBool(L, comboRef, "_lastAerialHitLanded", aerialHit)) {
                if (wrote) line += ",";
                line += "\"aerial_hit\":"; line += aerialHit ? "true" : "false";
                wrote = true;
            }
            bool stringHit = false;
            if (FieldBool(L, comboRef, "_aerialStringHit", stringHit)) {
                if (wrote) line += ",";
                line += "\"string_hit\":"; line += stringHit ? "true" : "false";
                wrote = true;
            }
            if (FieldNumber(L, comboRef, "_stateTimer", stateTimer)) {
                if (wrote) line += ",";
                line += "\"state_t\":"; AppendNumber(line, stateTimer, 2);
                wrote = true;
            }
            // Which of ComboManager's swing variants is live, so a recording
            // can confirm it is of the variant it asked for.
            std::string variant;
            if (FieldString(L, comboRef, "SwingVariant", variant)) {
                if (wrote) line += ",";
                line += "\"variant\":"; AppendEscaped(line, variant);
                wrote = true;
            }
            if (wrote) line += ",";
            line += "\"entered\":[";
            for (std::size_t i = 0; i < g_comboEntered.size(); ++i) {
                const ComboEntry& e = g_comboEntered[i];
                if (i) line += ",";
                line += "{\"state\":"; AppendEscaped(line, e.state);
                line += ",\"frame\":"; line += std::to_string(e.frame);
                line += ",\"mono\":"; AppendNumber(line, e.mono, 4);
                line += "}";
            }
            line += "],\"queued_frames\":"; line += std::to_string(g_comboQueuedFrames);
            wrote = true;
            if (GlobalBool(L, "player_can_move", canMove)) {
                if (wrote) line += ",";
                line += "\"can_move\":"; line += canMove ? "true" : "false";
                wrote = true;
            }
            // The gate on every aerial attack. ComboManager refuses one below
            // MinAerialAttackHeight, so this is what says whether the aerial
            // moveset is reachable with the jump the level actually ships.
            double airNow = 0.0;
            bool jumping = false;
            if (GlobalNumber(L, "player_air_height", airNow)) {
                if (wrote) line += ",";
                line += "\"air_h\":"; AppendNumber(line, airNow, 3);
                line += ",\"air_peak\":"; AppendNumber(line, g_airHeightPeak, 3);
                wrote = true;
            }
            if (GlobalBool(L, "player_is_jumping", jumping)) {
                if (wrote) line += ",";
                line += "\"jumping\":"; line += jumping ? "true" : "false";
                wrote = true;
            }
            bool grounded = false;
            if (GlobalBool(L, "player_is_grounded", grounded)) {
                if (wrote) line += ",";
                line += "\"grounded\":"; line += grounded ? "true" : "false";
                wrote = true;
            }
            // The jump is also refused while landing, and while attacking the
            // combo owns the input, so both are needed to tell "the jump was
            // blocked" from "the press was never seen".
            bool landing = false, attacking = false;
            if (GlobalBool(L, "player_is_landing", landing)) {
                if (wrote) line += ",";
                line += "\"landing\":"; line += landing ? "true" : "false";
                wrote = true;
            }
            if (GlobalBool(L, "player_is_attacking", attacking)) {
                if (wrote) line += ",";
                line += "\"attacking\":"; line += attacking ? "true" : "false";
                wrote = true;
            }
            // PlayerMovement returns before the jump section while this is
            // set, so it can stop the player jumping without anything else
            // in the state saying why.
            bool nearInteract = false;
            if (GlobalBool(L, "playerNearInteractable", nearInteract)) {
                if (wrote) line += ",";
                line += "\"near_interactable\":"; line += nearInteract ? "true" : "false";
                wrote = true;
            }
            // Which gate refused the jump, named by PlayerMovement at the gate
            // itself. Every gate reads clear from the outside while the jump is
            // still refused about half the time after a fight, so the answer
            // has to come from inside the check rather than from around it.
            std::string jumpBlock;
            if (GlobalString(L, "player_jump_block", jumpBlock)) {
                if (wrote) line += ",";
                line += "\"jump_block\":"; AppendEscaped(line, jumpBlock);
                wrote = true;
            }
            bool stun = false;
            if (GlobalBool(L, "player_is_damage_stun", stun)) {
                if (wrote) line += ",";
                line += "\"stun\":"; line += stun ? "true" : "false";
            }
            line += "}";
            g_airHeightPeak = 0.0;
        }
        g_comboEntered.clear();
        g_comboQueuedFrames = 0;

        line += ",\"enemies\":[";
        bool first = true;
        int alive = 0;
        for (const auto& [a, kind] : enemies) {
            if (!first) line += ",";
            first = false;
            AppendActorCommon(line, a);

            line += ",\"kind\":";
            AppendEscaped(line, kind == kMinibossScript ? "Miniboss" : "Enemy");

            if (kind == kMinibossScript) {
                // Phase and current move. Move5, Death Lotus, is weighted only
                // in phase 4, and _ComputePhase returned 1, 2 or 3, so the
                // question these answer is whether the finisher can now be
                // rolled at all.
                double phase = 0.0;
                if (FieldNumber(L, a.instanceRef, "_phase", phase)) {
                    line += ",\"phase\":"; AppendNumber(line, phase, 0);
                }
                std::string moveKind;
                if (FieldTableEntryString(L, a.instanceRef, "_move", "kind", moveKind)) {
                    line += ",\"move\":"; AppendEscaped(line, moveKind);
                }
                // How far into its move the boss is, whether the move's
                // damage has gone out yet, and the stagger cooldown. A hit
                // may stop an attack before its damage at most once per
                // cooldown, and these are what show whether it did.
                double moveStep = 0.0;
                if (FieldNestedNumber(L, a.instanceRef, "_move", "step", moveStep)) {
                    line += ",\"move_step\":"; AppendNumber(line, moveStep, 0);
                }
                bool landed = false, flag = false;
                for (const char* key : { "didHit", "slashed", "didFire" }) {
                    if (FieldNestedBool(L, a.instanceRef, "_move", key, flag) && flag) landed = true;
                }
                line += ",\"move_landed\":"; line += landed ? "true" : "false";
                double staggerCd = 0.0, throwsAway = 0.0;
                if (FieldNumber(L, a.instanceRef, "_staggerCdT", staggerCd)) {
                    line += ",\"stagger_cd\":"; AppendNumber(line, staggerCd, 2);
                }
                if (FieldNumber(L, a.instanceRef, "_p1ThrowsAway", throwsAway)) {
                    line += ",\"throws_away\":"; AppendNumber(line, throwsAway, 0);
                }
                double ticks = 0.0, seenX = 0.0, seenZ = 0.0, selfX = 0.0, selfZ = 0.0;
                bool sees = false;
                if (GlobalNumber(L, "miniboss_ticks", ticks)) {
                    line += ",\"ticks\":"; AppendNumber(line, ticks, 0);
                }
                bool frozen = false, introDone = false;
                if (GlobalBool(L, "miniboss_frozen", frozen)) {
                    line += ",\"frozen\":"; line += frozen ? "true" : "false";
                }
                if (GlobalBool(L, "miniboss_intro_done", introDone)) {
                    line += ",\"intro_done\":"; line += introDone ? "true" : "false";
                }
                if (GlobalBool(L, "miniboss_sees_player", sees)) {
                    line += ",\"sees_player\":"; line += sees ? "true" : "false";
                }
                if (GlobalNumber(L, "miniboss_player_x", seenX)
                    && GlobalNumber(L, "miniboss_player_z", seenZ)) {
                    line += ",\"sees_at\":["; AppendNumber(line, seenX, 2);
                    line += ","; AppendNumber(line, seenZ, 2); line += "]";
                }
                if (GlobalNumber(L, "miniboss_self_x", selfX)
                    && GlobalNumber(L, "miniboss_self_z", selfZ)) {
                    line += ",\"self_at\":["; AppendNumber(line, selfX, 2);
                    line += ","; AppendNumber(line, selfZ, 2); line += "]";
                }
            }

            std::string type;
            if (FieldString(L, a.instanceRef, "EnemyType", type)) {
                line += ",\"type\":";
                AppendEscaped(line, type);
                line += ",\"flying\":";
                line += IsFlyingType(type) ? "true" : "false";
            }
            double hp = 0.0;
            if (FieldNumber(L, a.instanceRef, "health", hp)) {
                line += ",\"hp\":"; AppendNumber(line, hp, 1);
            }
            bool dead = false;
            const bool haveDead = FieldBool(L, a.instanceRef, "dead", dead);
            if (haveDead) {
                line += ",\"dead\":";
                line += dead ? "true" : "false";
            }
            if (!haveDead || !dead) ++alive;

            std::string state;
            if (FieldNestedString(L, a.instanceRef, "fsm", "currentName", state)) {
                line += ",\"state\":";
                AppendEscaped(line, state);
            }
            // How long the AI has been in that state. A state whose age keeps
            // resetting towards zero is flapping, and flapping is invisible in
            // the state name alone: sampling only the name shows a steady
            // "Chase" whether the enemy has been chasing for five seconds or
            // has re-entered Chase twenty times in those five seconds.
            double stateAge = 0.0;
            if (FieldNestedNumber(L, a.instanceRef, "fsm", "timeInState", stateAge)) {
                line += ",\"state_age\":"; AppendNumber(line, stateAge, 2);
            }
            // Whether the current attack's hit has gone out. It turns true on
            // the frame a claw lands or a knife leaves, so the animation on
            // that frame is the one the hit came from.
            bool damageDealt = false;
            if (FieldBool(L, a.instanceRef, "_damageDealt", damageDealt)) {
                line += ",\"damage_dealt\":"; line += damageDealt ? "true" : "false";
            }
            if (a.haveAnim) {
                line += ",\"anim\":";
                AppendEscaped(line, a.animState);
                line += ",\"anim_clip\":";
                line += std::to_string(a.animClip);
                line += ",\"anim_asset\":";
                AppendEscaped(line, a.animAsset);
                line += ",\"anim_params\":";
                AppendEscaped(line, a.animParams);
                if (!a.animMiswired.empty()) {
                    line += ",\"anim_miswired\":";
                    AppendEscaped(line, a.animMiswired);
                }
            }

            // The flags that decide whether an enemy is allowed to leave its
            // Attack state. GroundAttackState only lets it go when
            // _attackCommitted and the anim-triggered flag are both clear, so
            // when an enemy sits in Attack indefinitely, exactly one of these
            // is stuck and this says which. Reading the code to work that out
            // produced two confident wrong answers, so it is reported instead.
            bool committed = false, meleeTrig = false, rangedTrig = false, dealt = false;
            double atkTimer = 0.0;
            const bool haveAtk =
                FieldBool(L, a.instanceRef, "_attackCommitted", committed)
                | FieldBool(L, a.instanceRef, "meleeAnimTriggered", meleeTrig)
                | FieldBool(L, a.instanceRef, "rangedAnimTriggered", rangedTrig)
                | FieldBool(L, a.instanceRef, "_damageDealt", dealt)
                | FieldNumber(L, a.instanceRef, "attackTimer", atkTimer);
            // The ranges the enemy is actually using, from its live config
            // table. Reading them out of the scene file gives two different
            // answers depending on which copy of the field you take, the same
            // way the feather prefab path did, so the running game is asked.
            double atkRange = 0.0, disengage = 0.0, meleeRange = 0.0;
            if (FieldNestedNumber(L, a.instanceRef, "config", "AttackRange", atkRange)
                | FieldNestedNumber(L, a.instanceRef, "config", "AttackDisengageRange", disengage)
                | FieldNestedNumber(L, a.instanceRef, "config", "MeleeRange", meleeRange)) {
                line += ",\"ranges\":{\"attack\":"; AppendNumber(line, atkRange, 2);
                line += ",\"disengage\":"; AppendNumber(line, disengage, 2);
                line += ",\"melee\":"; AppendNumber(line, meleeRange, 2);
                line += "}";
            }

            if (haveAtk) {
                line += ",\"atk\":{\"committed\":";
                line += committed ? "true" : "false";
                line += ",\"melee_trig\":";
                line += meleeTrig ? "true" : "false";
                line += ",\"ranged_trig\":";
                line += rangedTrig ? "true" : "false";
                line += ",\"dealt\":";
                line += dealt ? "true" : "false";
                line += ",\"timer\":"; AppendNumber(line, atkTimer, 2);
                line += "}";
            }
            line += "}";
        }
        line += "],\"enemies_alive\":";
        line += std::to_string(alive);
        line += "}\n";

        g_out << line;
        g_out.flush();  // the reader is another process tailing the file

        if (g_walkmapPeriod > 0.0 && havePlayer) {
            g_walkmapAccum += sinceLastSample;
            if (g_walkmapAccum >= g_walkmapPeriod) {
                g_walkmapAccum = 0.0;
                WriteWalkmap(player.pos);
            }
        }
    }

}  // namespace Telemetry
