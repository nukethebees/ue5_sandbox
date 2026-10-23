#pragma once
#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/collision/entity_aabbs.h>
#include <ioj/sim/collision/world_aabbs.h>
#include <ioj/sim/entities/entity_type.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <Engine/EngineTypes.h>
#include <UObject/WeakObjectPtrTemplates.h>

#include <cstddef>
#include <expected>

class UStaticMesh;
class UPrimitiveComponent;
class UWorld;
struct FCollisionGridConfig;

namespace ioj {
using FEntityBoundsExtractionResult = std::expected<sim::collision::EntityAABBs, FLevelStartErrors>;

struct FStaticCollisionSource {
    TWeakObjectPtr<UPrimitiveComponent> component;
    ECollisionEnabled::Type original_collision_mode{ECollisionEnabled::NoCollision};
};

struct SPACEGAME_API FLevelCollisionHost {
    using EntityMeshes = EnumArray<sim::EntityType, UStaticMesh const*>;
    static auto extract_entity_bounds(EntityMeshes const& meshes) -> FEntityBoundsExtractionResult;
    auto initialise_static_geometry(UWorld& world,
                                    FCollisionGridConfig const& config,
                                    sim::collision::GridGeometry grid_geometry)
        -> sim::collision::WorldAABBs;
    void restore_collision();
    auto get_static_collision_sources() const noexcept -> TConstArrayView<FStaticCollisionSource> {
        return static_collision_sources_;
    }
  private:
    TArray<FStaticCollisionSource> static_collision_sources_;
};
}
