#pragma once
#if WITH_EDITOR
#include <ioj/sim/collision/entity_aabbs.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/world_aabb.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <Containers/UnrealString.h>

#include <cstddef>
#include <expected>
#include <vector>

class UStaticMesh;
class ULevel;
class AActor;

namespace ioj {
struct FCollisionGridConfig;

using FEntityBoundsExtractionResult = std::expected<sim::collision::EntityAABBs, FLevelStartErrors>;

struct SPACEGAME_API FLevelCollisionHost {
    using EntityMeshes = EnumArray<UStaticMesh const*, sim::EntityType>;
    static auto extract_entity_bounds(EntityMeshes const& meshes) -> FEntityBoundsExtractionResult;
    static auto exports_actor(AActor const& actor, FCollisionGridConfig const& config) -> bool;
    static auto export_static_collision(ULevel const& level, FCollisionGridConfig const& config)
        -> std::expected<std::vector<WorldAABB>, FString>;
};
}
#endif
