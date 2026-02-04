#include "pch.h"
#include "Game AI/NavSystem.hpp"
#include <ECS/ECSRegistry.hpp>
#include "Physics/Kinematics/CharacterController.hpp"
#include "Physics/Kinematics/CharacterControllerSystem.hpp"
#include <ECS/NameComponent.hpp>

NavSystem& NavSystem::Get()
{
    static NavSystem instance;
    return instance;
}

void NavSystem::Build(PhysicsSystem& phys, ECSManager& ecsManager)
{
    grid.Build(phys, ecsManager);
    built = true;
}

std::vector<Vector3D> NavSystem::RequestPathXZ(float sx, float sz, float gx, float gz, Entity entity)
{
    if (!built) {
        ENGINE_PRINT("[NavSystem] ERROR: Grid not built yet!");
        return {};
    }

    auto [sr, sc] = grid.WorldToCell(sx, sz);
    auto [gr, gc] = grid.WorldToCell(gx, gz);

    ENGINE_PRINT("[NavSystem] Path request: start({:.2f},{:.2f}) -> cell[{},{}] walkable={}",
        sx, sz, sr, sc, grid.Walkable(sr, sc));
    ENGINE_PRINT("[NavSystem] Path request: goal({:.2f},{:.2f}) -> cell[{},{}] walkable={}",
        gx, gz, gr, gc, grid.Walkable(gr, gc));

    // If start is not walkable, snap the entity to nearest walkable node.
    if (!grid.Walkable(sr, sc)) {
        ENGINE_PRINT("[NavSystem] Start is NOT WALKABLE!");
		GridPos nearest = AStar::FindNearestWalkable(grid, { sr, sc });
		auto worldPos = grid.CellToWorld(nearest.row, nearest.col);

        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

		auto& transform = ecs.GetComponent<Transform>(entity);
        auto characterController = ecs.characterControllerSystem->GetController(entity);
        if (characterController != nullptr) {
            ecs.transformSystem->SetWorldPosition(entity, { worldPos.x, worldPos.y, worldPos.z });
            characterController->SetPosition(transform);
        }
        else {
		    ecs.transformSystem->SetWorldPosition(entity, { worldPos.x, worldPos.y, worldPos.z });
        }

        sx = transform.worldPosition.x;
        sz = transform.worldPosition.z;
        ENGINE_PRINT("[NavSystem] Snapped {} to world position: {:.2f}, {:.2f}, {:.2f}",
            ecs.GetComponent<NameComponent>(entity).name,
            transform.worldPosition.x, transform.worldPosition.y, transform.worldPosition.z);
        ENGINE_PRINT("[NavSystem] Snapped {} to local position: {:.2f}, {:.2f}, {:.2f}",
            ecs.GetComponent<NameComponent>(entity).name,
            transform.localPosition.x, transform.localPosition.y, transform.localPosition.z);
    }

    auto path = astar.FindPath(grid, sx, sz, gx, gz);

    ENGINE_PRINT("[NavSystem] Path result: {} waypoints={}",
        (path.empty() ? "FAILED" : "SUCCESS"), path.size());

    return path;
}

float NavSystem::GetGroundY(Entity entity) {
    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    auto& transform = ecs.GetComponent<Transform>(entity);
    Vector3D worldPos = transform.worldPosition;
    GridPos gridPos = grid.WorldToCell(worldPos.x, worldPos.z);
    //std::cout << "[NavSystem] GetGroundY gridPos row: " << gridPos.row << " col: " << gridPos.col << std::endl;
    return grid.GetNavCell(gridPos.row, gridPos.col).groundY;
}
