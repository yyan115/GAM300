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
        std::cout << "[NavSystem] ERROR: Grid not built yet!\n";
        return {};
    }

    auto [sr, sc] = grid.WorldToCell(sx, sz);
    auto [gr, gc] = grid.WorldToCell(gx, gz);

    //std::cout << "[NavSystem] Path request: start(" << sx << "," << sz
    //    << ") -> cell[" << sr << "," << sc << "] walkable=" << grid.Walkable(sr, sc) << "\n";
    //std::cout << "[NavSystem] Path request: goal(" << gx << "," << gz
    //    << ") -> cell[" << gr << "," << gc << "] walkable=" << grid.Walkable(gr, gc) << "\n";

    // If start is not walkable, snap the entity to nearest walkable node.
    if (!grid.Walkable(sr, sc)) {
        //std::cout << "[NavSystem] Start is NOT WALKABLE!" << std::endl;
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
        //std::cout << "[NavSystem] Snapped " << ecs.GetComponent<NameComponent>(entity).name << " to world position: " << transform.worldPosition.x << ", " << transform.worldPosition.y << ", " << transform.worldPosition.z << std::endl;
        //std::cout << "[NavSystem] Snapped " << ecs.GetComponent<NameComponent>(entity).name << " to local position: " << transform.localPosition.x << ", " << transform.localPosition.y << ", " << transform.localPosition.z << std::endl;
    }

    auto path = astar.FindPath(grid, sx, sz, gx, gz);

    //std::cout << "[NavSystem] Path result: " << (path.empty() ? "FAILED" : "SUCCESS")
    //    << " waypoints=" << path.size() << "\n";

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
