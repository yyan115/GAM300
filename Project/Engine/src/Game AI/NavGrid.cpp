#include "pch.h"
#include "Game AI/NavGrid.hpp"
#include "ECS/LayerManager.hpp"

#include "ECS/NameComponent.hpp"

// If these aren't already included via NavGrid.hpp, you need the correct headers:
// #include "ECS/ECSManager.hpp"
// #include "ECS/LayerComponent.hpp"
// #include "ECS/Transform.hpp"
// #include "Physics/ColliderComponent.hpp"

struct ObstacleFootprint
{
    float minX, maxX;
    float minZ, maxZ;
};

static const char* SafeName(ECSManager& ecs, Entity e)
{
    if (ecs.HasComponent<NameComponent>(e))
        return ecs.GetComponent<NameComponent>(e).name.c_str();
    return "<noname>";
}

static bool IsHitLayer(ECSManager& ecs, Entity e, int layerIdx)
{
    if (!ecs.HasComponent<LayerComponent>(e)) return false;
    return ecs.GetComponent<LayerComponent>(e).layerIndex == layerIdx;
}

static bool IsGroundHit(PhysicsSystem& phys,
    ECSManager& ecsManager,
    const JPH::BodyID& bodyId,
    int groundIdx)
{
    if (bodyId.IsInvalid())
        return false;

    Entity e = phys.GetEntityFromBody(bodyId);
    if ((int)e == 0)
        return false;

    if (!ecsManager.HasComponent<LayerComponent>(e))
        return false;

    return ecsManager.GetComponent<LayerComponent>(e).layerIndex == groundIdx;
}


// Keeping this for your original debug prints if you still want it elsewhere.
// (No longer used by Build after patch.)
static bool IsObstacleHit(PhysicsSystem& phys,
    ECSManager& ecsManager,
    const JPH::BodyID& bodyId,
    int obstacleIdx)
{
    if (bodyId.IsInvalid()) return false;

    Entity e = phys.GetEntityFromBody(bodyId);
    ENGINE_PRINT("[NavGrid] Hit body -> entity {} name={}", (int)e, SafeName(ecsManager, e));

    if (!ecsManager.HasComponent<LayerComponent>(e))
    {
        ENGINE_PRINT(" NO LayerComponent");
        return false;
    }

    const auto& lc = ecsManager.GetComponent<LayerComponent>(e);
    ENGINE_PRINT(" layerIndex={} (ObstacleIdx={})", lc.layerIndex, obstacleIdx);

    return lc.layerIndex == obstacleIdx;
}

static ObstacleFootprint MakeBoxFootprint(const Transform& tr, const ColliderComponent& col, float inflate)
{
    // World center (offset scaled)
    const float cx = tr.localPosition.x + col.center.x * tr.localScale.x;
    const float cz = tr.localPosition.z + col.center.z * tr.localScale.z;

    // Half extents in world
    const float hx = std::abs(col.boxHalfExtents.x * tr.localScale.x) + inflate;
    const float hz = std::abs(col.boxHalfExtents.z * tr.localScale.z) + inflate;

    ObstacleFootprint fp;
    fp.minX = cx - hx; fp.maxX = cx + hx;
    fp.minZ = cz - hz; fp.maxZ = cz + hz;
    return fp;
}

void NavGrid::Build(PhysicsSystem& phys, ECSManager& ecsManager)
{
    // ------------------------------------------------------------
    // 0) Early out if physics not ready
    // ------------------------------------------------------------
    if (!phys.IsJoltInitialized())
    {
        ENGINE_PRINT("[NavGrid] Build skipped: Jolt not initialized");
        return;
    }

    const int obstacleIdx = LayerManager::GetInstance().GetLayerIndex("Obstacle");
    const int groundIdx = LayerManager::GetInstance().GetLayerIndex("Ground");

    // ------------------------------------------------------------
    // 1) Helper lambdas
    // ------------------------------------------------------------
    auto GetLayerIndexOrNeg1 = [&](Entity e) -> int
        {
            if (!ecsManager.HasComponent<LayerComponent>(e)) return -1;
            return ecsManager.GetComponent<LayerComponent>(e).layerIndex;
        };

    auto GetNameOrEmpty = [&](Entity e) -> const std::string&
        {
            static const std::string kEmpty;
            if (!ecsManager.HasComponent<NameComponent>(e)) return kEmpty;
            return ecsManager.GetComponent<NameComponent>(e).name;
        };

    auto HasNameLike = [&](Entity e, const char* a, const char* b) -> bool
        {
            if (!ecsManager.HasComponent<NameComponent>(e)) return false;
            const std::string& n = ecsManager.GetComponent<NameComponent>(e).name;
            return (n.find(a) != std::string::npos) || (b && n.find(b) != std::string::npos);
        };

    // IMPORTANT: exclude "TempGround" / "GroundStep" explicitly
    auto IsAllowedGroundEntity = [&](Entity e) -> bool
        {
            // must be in Ground layer
            if (GetLayerIndexOrNeg1(e) != groundIdx)
                return false;

            // reject special cases by name
            const std::string& n = GetNameOrEmpty(e);
            if (!n.empty())
            {
                if (n.find("TempGround") != std::string::npos) return false;
                if (n.find("GroundStep") != std::string::npos) return false;
                if (n.find("Step") != std::string::npos) return false; // optional extra guard
            }

            return true;
        };

    // ------------------------------------------------------------
    // 2) Derive nav bounds from allowed Ground AABB(s)
    // ------------------------------------------------------------
    bool hasGroundBounds = false;
    float gMinX = 0, gMaxX = 0, gMinZ = 0, gMaxZ = 0;

    int groundPrinted = 0;
    for (Entity e : ecsManager.GetAllEntities())
    {
        if (!IsAllowedGroundEntity(e)) continue;

        JPH::AABox box;
        if (!phys.GetBodyWorldAABB(e, box))
            continue;

        const float minx = box.mMin.GetX();
        const float maxx = box.mMax.GetX();
        const float minz = box.mMin.GetZ();
        const float maxz = box.mMax.GetZ();

        if (!hasGroundBounds)
        {
            gMinX = minx; gMaxX = maxx; gMinZ = minz; gMaxZ = maxz;
            hasGroundBounds = true;
        }
        else
        {
            gMinX = std::min(gMinX, minx);
            gMaxX = std::max(gMaxX, maxx);
            gMinZ = std::min(gMinZ, minz);
            gMaxZ = std::max(gMaxZ, maxz);
        }

        if (groundPrinted < 8)
        {
            ENGINE_PRINT("[NavGrid] Allowed Ground AABB: {} X({:.2f},{:.2f}) Z({:.2f},{:.2f}) ecsLayer={}",
                SafeName(ecsManager, e), minx, maxx, minz, maxz, GetLayerIndexOrNeg1(e));
            groundPrinted++;
        }
    }

    if (hasGroundBounds)
    {
        const float PAD = 0.0f; // keep small; grid already tiny
        minX = gMinX - PAD;
        maxX = gMaxX + PAD;
        minZ = gMinZ - PAD;
        maxZ = gMaxZ + PAD;

        ENGINE_PRINT("[NavGrid] Using ground-derived bounds X({:.2f},{:.2f}) Z({:.2f},{:.2f})", minX, maxX, minZ, maxZ);
    }
    else
    {
        ENGINE_PRINT("[NavGrid] WARNING: No allowed ground bounds found via physics AABB.\n          Using configured bounds (may include unreachable areas).");
    }

    // ------------------------------------------------------------
    // 3) Setup grid storage (AFTER bounds)
    // ------------------------------------------------------------
    cols = static_cast<int>(std::ceil((maxX - minX) / cellSize));
    rows = static_cast<int>(std::ceil((maxZ - minZ) / cellSize));
    cells.assign(rows * cols, {});

    ENGINE_PRINT("[NavGrid] Build begin | bounds X({:.2f},{:.2f}) Z({:.2f},{:.2f}) cellSize={:.2f} cols={} rows={} GroundIdx={} ObstacleIdx={}",
        minX, maxX, minZ, maxZ, cellSize, cols, rows, groundIdx, obstacleIdx);

    // ------------------------------------------------------------
    // 4) Bake tuning
    // ------------------------------------------------------------
    constexpr float NAV_RADIUS_INFLATION = 0.15f;
    constexpr float NAV_HEIGHT_INFLATION = 0.05f;

    const float AABB_INFLATE = agentRadius * 0.5f;

    const float PROBE_TOP = std::max(groundProbeTop, 50.0f);
    const float PROBE_DIST = std::max(groundProbeDist, 200.0f);

    // ------------------------------------------------------------
    // 5) Gather Obstacle AABBs (stamp)
    // ------------------------------------------------------------
    struct AABB2D { float minX, maxX, minZ, maxZ; };
    std::vector<AABB2D> obsAABBs;
    obsAABBs.reserve(64);

    int obsPrinted = 0;
    for (Entity e : ecsManager.GetAllEntities())
    {
        if (GetLayerIndexOrNeg1(e) != obstacleIdx) continue;

        JPH::AABox box;
        if (!phys.GetBodyWorldAABB(e, box))
            continue;

        AABB2D a;
        a.minX = box.mMin.GetX() - AABB_INFLATE;
        a.maxX = box.mMax.GetX() + AABB_INFLATE;
        a.minZ = box.mMin.GetZ() - AABB_INFLATE;
        a.maxZ = box.mMax.GetZ() + AABB_INFLATE;
        obsAABBs.push_back(a);

        if (obsPrinted < 12)
        {
            ENGINE_PRINT("[NavGrid] Obstacle AABB: {} X({:.2f},{:.2f}) Z({:.2f},{:.2f})",
                SafeName(ecsManager, e), a.minX, a.maxX, a.minZ, a.maxZ);
            obsPrinted++;
        }
    }

    ENGINE_PRINT("[NavGrid] obstacleAABBs={} AABB_INFLATE={:.2f} agentRadius={:.2f} agentHalfHeight={:.2f}",
        obsAABBs.size(), AABB_INFLATE, agentRadius, agentHalfHeight);

    // ------------------------------------------------------------
    // 6) Debug Rays (3 points)
    // ------------------------------------------------------------
    auto DebugRay = [&](float x, float z)
        {
            Vector3D o(x, PROBE_TOP, z);
            Vector3D d(0, -1, 0);

            auto any = phys.Raycast(o, d, PROBE_DIST);
            if (any.hit)
            {
                Entity e = phys.GetEntityFromBody(any.bodyId);
                ENGINE_PRINT("[NavGrid] ANY ({:.2f},{:.2f},{:.2f}) hit=true y={:.2f} ent={} name={} ecsLayer={}",
                    x, PROBE_TOP, z, any.hitPoint.y, (int)e, SafeName(ecsManager, e), GetLayerIndexOrNeg1(e));
            }
            else
            {
                ENGINE_PRINT("[NavGrid] ANY ({:.2f},{:.2f},{:.2f}) hit=false", x, PROBE_TOP, z);
            }

            auto g = phys.RaycastGround(o, d, PROBE_DIST, ecsManager,
                groundIdx, obstacleIdx,
                /*acceptObstacleAsHit=*/false,
                /*debugLog=*/true);

            if (g.hit)
            {
                Entity e = phys.GetEntityFromBody(g.bodyId);
                ENGINE_PRINT("[NavGrid] GROUNDONLY ({:.2f},{:.2f},{:.2f}) hit=true y={:.2f} ent={} name={} ecsLayer={}",
                    x, PROBE_TOP, z, g.hitPoint.y, (int)e, SafeName(ecsManager, e), GetLayerIndexOrNeg1(e));
            }
            else
            {
                ENGINE_PRINT("[NavGrid] GROUNDONLY ({:.2f},{:.2f},{:.2f}) hit=false", x, PROBE_TOP, z);
            }
        };

    DebugRay(0.0f, 0.0f);
    DebugRay(-2.0f, -2.0f);
    DebugRay(2.0f, 2.0f);

    // ------------------------------------------------------------
    // 7) Main bake + counters + ASCII reason map
    // ------------------------------------------------------------
    int blockedStamp = 0;
    int blockedNoGround = 0;
    int blockedOverlap = 0;
    int walkableCount = 0;

    // reasonMap holds one char per cell
    // '.' walkable, 'S' stamp, 'N' no ground, 'O' overlap
    std::vector<char> reasonMap(rows * cols, '?');

    // Optional: log first few NoGround cells
    constexpr int MAX_NOGROUND_LOGS = 20;
    int noGroundLogs = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const float x = minX + (c + 0.5f) * cellSize;
            const float z = minZ + (r + 0.5f) * cellSize;

            NavCell& cell = cells[r * cols + c];

            // A) Obstacle stamp hard block
            bool insideObstacle = false;
            for (const auto& a : obsAABBs)
            {
                if (x >= a.minX && x <= a.maxX && z >= a.minZ && z <= a.maxZ)
                {
                    insideObstacle = true;
                    break;
                }
            }

            if (insideObstacle)
            {
                //std::cout << "[NavGrid] Cell not walkable r=" << r << " c=" << c
                //    << " x=" << x << " z=" << z
                //    << " probeTop=" << PROBE_TOP
                //    << " probeDist=" << PROBE_DIST
                //    << "\n";

                cell.walkable = false;
                reasonMap[r * cols + c] = 'S';
                blockedStamp++;
                continue;
            }

            // B) Ground raycast (Ground layer ONLY)
            Vector3D rayOrigin(x, PROBE_TOP, z);
            Vector3D rayDir(0, -1, 0);

            PhysicsSystem::RaycastResult hit =
                phys.RaycastGround(rayOrigin, rayDir, PROBE_DIST, ecsManager,
                    groundIdx, obstacleIdx,
                    /*acceptObstacleAsHit=*/false,
                    /*debugLog=*/false);

            if (!hit.hit)
            {
                cell.walkable = false;
                reasonMap[r * cols + c] = 'N';
                blockedNoGround++;

                if (noGroundLogs < MAX_NOGROUND_LOGS)
                {
                    ENGINE_PRINT("[NavGrid] NoGround r={} c={} x={:.2f} z={:.2f} probeTop={:.2f} probeDist={:.2f}",
                        r, c, x, z, PROBE_TOP, PROBE_DIST);
                    noGroundLogs++;
                }
                continue;
            }

            cell.groundY = hit.hitPoint.y;
			//std::cout << "[NavGrid] Row " << r << " Col " << c << " groundY = " << cell.groundY << "\n";

            // C) Clearance overlap against obstacles
            const float navRadius = agentRadius + NAV_RADIUS_INFLATION;
            const float navHalfHeight = agentHalfHeight + NAV_HEIGHT_INFLATION;

            Vector3D capsuleCenter(
                x,
                cell.groundY + navHalfHeight + navRadius,
                z
            );

            const bool blockedByObstacle =
                phys.OverlapCapsuleObstacleLayer(
                    capsuleCenter,
                    navHalfHeight,
                    navRadius,
                    ecsManager,
                    obstacleIdx
                );

            if (blockedByObstacle)
            {
                cell.walkable = false;
                reasonMap[r * cols + c] = 'O';
                blockedOverlap++;
                continue;
            }

            // Walkable
            cell.walkable = true;
            reasonMap[r * cols + c] = '.';
            walkableCount++;
        }
    }

    // ------------------------------------------------------------
    // 8) Print summary + ASCII map
    // ------------------------------------------------------------
    const int total = rows * cols;
    auto pct = [&](int n) -> float { return total > 0 ? (100.0f * float(n) / float(total)) : 0.0f; };

    ENGINE_PRINT("[NavGrid] Build done | grid={} walkable={} ({:.1f}%) stamp={} ({:.1f}%) noGround={} ({:.1f}%) overlap={} ({:.1f}%)",
        total, walkableCount, pct(walkableCount), blockedStamp, pct(blockedStamp),
        blockedNoGround, pct(blockedNoGround), blockedOverlap, pct(blockedOverlap));

    //std::cout << "[NavGrid] ASCII map legend: '.' walkable, 'S' stamp, 'N' noGround, 'O' overlap\n";
    //std::cout << "[NavGrid] row0 is MIN-Z side (r=0). If you want flipped vertically, print rows reversed.\n";

    //for (int r = 0; r < rows; ++r)
    //{
    //    std::cout << "[NavGrid] r=" << r << " ";
    //    for (int c = 0; c < cols; ++c)
    //    {
    //        std::cout << reasonMap[r * cols + c];
    //    }
    //    std::cout << "\n";
    //}
}

bool NavGrid::InBounds(int r, int c) const
{
    return r >= 0 && r < rows && c >= 0 && c < cols;
}

bool NavGrid::Walkable(int r, int c) const
{
    if (!InBounds(r, c)) {
        ENGINE_PRINT("[NavGrid::Walkable] Out of bounds: [{},{}] (bounds: 0-{}, 0-{})",
            r, c, (rows - 1), (cols - 1));
        return false;
    }

    bool result = cells[r * cols + c].walkable;

    // Debug spam reduction - only log non-walkable cells
    if (!result) {
        static int logCount = 0;
        if (logCount < 10) {
            ENGINE_PRINT("[NavGrid::Walkable] Cell [{},{}] is NOT walkable", r, c);
            logCount++;
        }
    }

    return result;
}

GridPos NavGrid::WorldToCell(float x, float z) const
{
    int c = static_cast<int>(std::floor((x - minX) / cellSize));
    int r = static_cast<int>(std::floor((z - minZ) / cellSize));

    c = std::clamp(c, 0, cols - 1);
    r = std::clamp(r, 0, rows - 1);

    //std::cout << "[WorldToCell] world(" << x << "," << z << ") -> cell["
    //    << r << "," << c << "] (clamped)\n";

    return GridPos{ r, c };
}

Vector3D NavGrid::CellToWorld(int r, int c) const
{
    float x = minX + (c + 0.5f) * cellSize;
    float z = minZ + (r + 0.5f) * cellSize;

    float y = 0.0f;
    if (InBounds(r, c))
        y = cells[r * cols + c].groundY;

    return Vector3D(x, y, z);
}

const NavCell& NavGrid::GetNavCell(int row, int col) {
    if (row >= 0 && row < rows &&
        col >= 0 && col < cols) {
        return cells[row * cols + col];
    }

    // Default case: return cells[0]
    //std::cout << "[NavCell] GetNavCell out of bounds. Returning cells[0]" << std::endl;
    return cells[0];
}
