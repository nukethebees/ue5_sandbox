#pragma once

#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/DrawDebugConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestCapitalShips.generated.h"

class ADelayedNiagaraSpawner;
class ATestCapitalShipProxy;
class UInstancedStaticMeshComponent;

UCLASS()
class SPACEGAME_API ATestCapitalShips : public AActor {
    GENERATED_BODY()
    friend class ATestBatchOrchestrator;
  public:
    using SpawnData = ml::test_capital_ships::SpawnData;
    using Proxy = ATestCapitalShipProxy;

    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    ATestCapitalShips();

    void set_actor_config(FCapitalShipConfig const* new_config) noexcept;
  private:
    void bind_simulation(ml::test_capital_ships::Simulation& new_simulation);
    auto simulation() -> ml::test_capital_ships::Simulation&;
    auto simulation() const -> ml::test_capital_ships::Simulation const&;
    void set_niagara_spawner(ADelayedNiagaraSpawner& spawner);

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void register_all_proxies_in_level();
    void bind_proxy_entities(FProxyEntityMap const& proxy_entities);
    void configure_ismc();
    void add_initial_visual_instances(SpawnData const& spawn_data);
    void trigger_death_effects();
    void draw_debugging_shapes() const;
    void visual_log_state() const;
    void validate_array_sizes() const;

    FCapitalShipConfig const* actor_config{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ADelayedNiagaraSpawner> niagara_spawner{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    bool debugging_shapes_enabled{false};

    ml::test_capital_ships::Simulation* bound_simulation{nullptr};
};
