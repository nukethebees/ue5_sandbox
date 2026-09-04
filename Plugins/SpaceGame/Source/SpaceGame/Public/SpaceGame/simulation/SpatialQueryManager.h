#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/defences/spinners/TestTubeSpinners.h>
#include <SpaceGame/defences/turrets/TestStaticTurrets.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/CollisionSystem.h>
#include <SpaceGame/simulation/LineTraces.h>
#include <SpaceGame/simulation/TraceHits.h>

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <Containers/BitArray.h>
#include <CoreMinimal.h>
#include <SandboxCore/soa_vectors.h>

#include <mutex>
#include <utility>

class UWorld;
class UPrimitiveComponent;
struct FTestEntityRegistry;
struct FCollisionGridConfig;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::query_manager {
struct FThreadBuffers {
    FLineTraces line_traces;
    FTraceHits trace_hits;
    TBitArray<> range_query_seen_entities;
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
struct FLineTraceResult {
    FVector3f location{FVector3f::ZeroVector};
    FRegistryEntityHandle entity;
    int32 static_geometry_index{INDEX_NONE};
    bool hit{false};
};

struct SPACEGAME_API FSpatialQueryManager {
  public:
    FSpatialQueryManager() = default;

    void initialise(FTestEntityRegistry const& entity_registry,
                    FCollisionGridConfig const& collision_grid_config,
                    UWorld& world,
                    ATestSpaceShip const* player_ship,
                    ATestCapitalShips const& capital_ships,
                    ATestCapitalShipFighters const& capital_ship_fighters,
                    ATestStaticTurrets const& static_turrets,
                    ATestTubeSpinners const& tube_spinners);

    void reserve_thread_buffers(int32 count);
    void initialise_static_geometry(FCollisionGridConfig const& collision_grid_config);
    auto add_static_geometry(UPrimitiveComponent& component) -> bool;

    void trace_line_of_sight(FVectors3f::ConstView start_locations,
                             FVectors3f::ConstView end_locations,
                             TArrayView<FRegistryEntityHandle> out_entity_handles) const;
    void has_line_of_sight_to_targets(FVector3f const& start_location,
                                      FVectors3f::ConstView end_locations,
                                      TConstArrayView<FRegistryEntityHandle> targets,
                                      TArrayView<uint8> has_los) const;
    void have_clear_lines(FVectors3f::ConstView start_locations,
                          FVectors3f::ConstView end_locations,
                          TArrayView<uint8> clear_lines,
                          TConstArrayView<FRegistryEntityHandle> ignored_entities = {}) const;
    auto has_clear_line(FVector3f start_location,
                        FVector3f end_location,
                        FRegistryEntityHandle ignored_entity = {}) const -> bool;
    auto trace_closest(FVector3f start_location,
                       FVector3f end_location,
                       FRegistryEntityHandle ignored_entity = {}) const -> FLineTraceResult;

    auto collect_non_team_entities_in_range(
        FVector3f const& origin,
        ETestTeam const team,
        float const radius,
        TArrayView<FRegistryEntityHandle> const out_entities) const -> int32;
    auto get_any_non_team_entity(ETestTeam const team) const -> FRegistryEntityHandle;
    auto get_any_non_team_entity(ETestTeam const team, ETestEntityType const entity_type) const
        -> FRegistryEntityHandle;

    auto get_collision_system() noexcept -> ioj::FCollisionSystem& { return collision; }
    auto get_collision_system() const noexcept -> ioj::FCollisionSystem const& { return collision; }

    void update();
  private:
    friend class query_manager::FThreadBufferLease;

    using FThreadBuffers = query_manager::FThreadBuffers;

    auto acquire_thread_buffer() const -> int32;
    void release_thread_buffer(int32 index) const;

    FTestEntityRegistry const* entity_registry{nullptr};
    UWorld* world{nullptr};

    mutable std::mutex thread_buffers_mutex;
    mutable TArray<FThreadBuffers> thread_buffers;
    mutable TArray<int32> free_thread_buffer_indices;
    mutable int32 active_thread_buffer_count{};

    ioj::FCollisionSystem collision;
};
}
