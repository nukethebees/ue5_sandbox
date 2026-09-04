#pragma once

#include <SpaceGame/simulation/collision_uniform_grid.h>
#include <SpaceGame/simulation/EntityAABBs.h>

#include <Containers/StaticArray.h>
#include <Engine/EngineTypes.h>
#include <UObject/WeakObjectPtrTemplates.h>

class AActor;
class UPrimitiveComponent;
class UStaticMesh;
class UWorld;
struct FTestEntityRegistry;
struct FCollisionGridConfig;

namespace ml::ioj {
struct FStaticCollisionSource {
    TWeakObjectPtr<AActor> actor;
    TWeakObjectPtr<UPrimitiveComponent> component;
    FTransform harvested_transform;
    ECollisionEnabled::Type original_collision_enabled{ECollisionEnabled::NoCollision};
};

struct SPACEGAME_API FCollisionSystem {
  public:
    using EntityMeshes = TStaticArray<UStaticMesh const*, FEntityAABBs::num_rows>;

    FCollisionSystem() = default;

    void initialise(EntityMeshes const& meshes);
    void initialise_static_geometry(UWorld& world, FCollisionGridConfig const& config);
    void update();

    auto get_entity_aabbs() const noexcept -> FEntityAABBs const& { return entity_aabbs_; }
    auto get_uniform_grid() noexcept -> CollisionUniformGrid& { return uniform_grid_; }
    auto get_uniform_grid() const noexcept -> CollisionUniformGrid const& { return uniform_grid_; }
    auto get_static_collision_sources() const noexcept -> TConstArrayView<FStaticCollisionSource> {
        return static_collision_sources_;
    }

    void set_entity_registry(FTestEntityRegistry const& registry);
  private:
    void rebuild_grid();

    FTestEntityRegistry const* entity_registry_{nullptr};
    CollisionUniformGrid uniform_grid_{};

    FEntityAABBs entity_aabbs_{};
    TArray<FStaticCollisionSource> static_collision_sources_;
};
}
