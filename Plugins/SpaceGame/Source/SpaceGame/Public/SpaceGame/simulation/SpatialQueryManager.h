#pragma once

#include <SpaceGame/simulation/SpatialQueryHit.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/defences/turrets/TestStaticTurrets.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/defences/spinners/TestTubeSpinners.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>
#include <SandboxCore/soa_vectors.h>

#include <mutex>
#include <utility>

class UPrimitiveComponent;
class UWorld;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::query_manager {
enum class EHitResolverKind : uint8 {
    PlayerShipMesh,
    CapitalShipInstances,
    CapitalShipFighterInstances,
    StaticTurretInstances,
    TubeSpinnerInstances,
    Count,
    Unknown,
};

struct FComponentResolver {
    UPrimitiveComponent const* component{nullptr};
    EHitResolverKind kind{EHitResolverKind::Unknown};
};

struct FThreadBuffers {
    TArray<FSpatialQueryHit> line_of_sight_hits;
    TArray<FSpatialQueryHit> sorted_line_of_sight_hits;
    TArray<FRegistryEntityHandle> sorted_line_of_sight_entity_handles;
    TArray<int32> line_of_sight_sort_indices;
};

class FThreadBufferLease {
  public:
    explicit FThreadBufferLease(FSpatialQueryManager const& manager);
    ~FThreadBufferLease();

    FThreadBufferLease(FThreadBufferLease const&) = delete;
    auto operator=(FThreadBufferLease const&) -> FThreadBufferLease& = delete;

    auto get() const -> FThreadBuffers&;
  private:
    FSpatialQueryManager const* manager;
    int32 index;
};
}

namespace ml {
struct SPACEGAME_API FSpatialQueryManager {
  public:
    // Required only for Unreal's default-construction path.
    // Pointer members are otherwise required to be non-null.
    // Explicitly default-constructing this type violates its contract.
    FSpatialQueryManager() = default;
    explicit FSpatialQueryManager(FTestEntityRegistry const& entity_registry) noexcept;

    void initialise(UWorld& world,
                    ATestSpaceShip const* player_ship,
                    ATestCapitalShips const& capital_ships,
                    ATestCapitalShipFighters const& capital_ship_fighters,
                    ATestStaticTurrets const& static_turrets,
                    ATestTubeSpinners const& tube_spinners);

    void reserve_thread_buffers(int32 count);

    void resolve_hits(TConstArrayView<FSpatialQueryHit> hits,
                      TArrayView<FRegistryEntityHandle> out_entity_handles) const;
    void trace_line_of_sight(FVectors3f::ConstView start_locations,
                             FVectors3f::ConstView end_locations,
                             TArrayView<FRegistryEntityHandle> out_entity_handles) const;
    void has_line_of_sight_to_targets(FVector3f const& start_location,
                                      FVectors3f::ConstView end_locations,
                                      TConstArrayView<FRegistryEntityHandle> targets,
                                      TArrayView<uint8> has_los) const;

    auto collect_non_team_entities_in_range(
        FVector3f const& origin,
        ETestTeam const team,
        float const radius,
        TArrayView<FRegistryEntityHandle> const out_entities) const -> int32;
    auto get_any_non_team_entity(ETestTeam const team) const -> FRegistryEntityHandle;
    auto get_any_non_team_entity(ETestTeam const team, ETestEntityType const entity_type) const
        -> FRegistryEntityHandle;
    void are_entities_within_dist_sq(float const dist_sq,
                                     FVectors3f const& locations,
                                     TArrayView<bool> const results) const;

    void update();
  private:
    friend class query_manager::FThreadBufferLease;

    using EHitResolverKind = query_manager::EHitResolverKind;
    using FComponentResolver = query_manager::FComponentResolver;
    using FThreadBuffers = query_manager::FThreadBuffers;

    auto classify_component(UPrimitiveComponent const* component) const -> EHitResolverKind;
    auto acquire_thread_buffer() const -> int32;
    void release_thread_buffer(int32 index) const;
    void resolve_line_of_sight_hits(query_manager::FThreadBuffers& buffers, int32 count) const;

    using ComponentResolvers =
        TArray<FComponentResolver, TInlineAllocator<std::to_underlying(EHitResolverKind::Count)>>;
    FTestEntityRegistry const* const entity_registry{nullptr};
    UWorld* world{nullptr};
    ComponentResolvers component_resolvers;
    mutable std::mutex thread_buffers_mutex;
    mutable TArray<FThreadBuffers> thread_buffers;
    mutable TArray<int32> free_thread_buffer_indices;
    mutable int32 active_thread_buffer_count{};
    FTestSpaceShipSpatialQueryAccess player_ship_access;
    FTestCapitalShipsSpatialQueryAccess capital_ships_access;
    FTestCapitalShipFightersSpatialQueryAccess capital_ship_fighters_access;
    FTestStaticTurretsSpatialQueryAccess static_turrets_access;
    FTestTubeSpinnersSpatialQueryAccess tube_spinners_access;
};
}
