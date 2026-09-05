#pragma once

#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGame/presentation/HUDManager.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestBatchOrchestrator.generated.h"

class ATestSpaceShip;
class UCollisionGridVisualizationComponent;
class USandboxISMCComponent;

class ATestBatchOrchestrator;

DECLARE_DELEGATE_OneParam(FOrchestratorEndTickTestHook, ATestBatchOrchestrator&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnProxyEntitiesBound, FProxyEntityMap const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOrchestratorReset, ATestBatchOrchestrator&);

UENUM(BlueprintType)
enum class EOrchestratorStartMode : uint8 {
    Paused,
    PausedInTest,
    Automatic,
    AuthoredLevel,
};

UCLASS()
class SPACEGAME_API ATestBatchOrchestrator : public AActor {
    GENERATED_BODY()
  public:
    using tick_type = uint64;
    using time_type = double;

    ATestBatchOrchestrator();

    void Tick(float dt) override;
    void PostLoad() override;
    void tick(time_type const dt);
    void start_simulation();
    void pause_simulation();
    void reset_for_new_level();

    void set_level_config(USpaceGameLevelConfig& config);
    auto get_level_config() const noexcept -> USpaceGameLevelConfig const* {
        return level_config.Get();
    }
    void set_start_mode(EOrchestratorStartMode mode);
    void set_presentation_enabled(bool enabled);
    void set_level_definition(ml::FLevelDefinition definition) {
        level_definition_ = MoveTemp(definition);
    }
    auto is_presentation_enabled() const noexcept -> bool { return presentation_enabled; }
    void set_time_scale(time_type scale) noexcept;

    auto frequency_to_tick_period(time_type const frequency) const noexcept -> tick_type;
    auto duration_to_tick_period(time_type const duration) const noexcept -> tick_type;

    auto get_state() const noexcept -> EOrchestratorState {
        return level_simulation_.IsSet() ? level_simulation_->get_state()
                                         : EOrchestratorState::Uninitialised;
    }
    auto get_completed_ticks() const noexcept -> tick_type {
        return level_simulation_.IsSet() ? level_simulation_->get_clock().get_completed_ticks() : 0;
    }
    auto get_simulation_time() const noexcept -> time_type {
        return level_simulation_.IsSet() ? level_simulation_->get_clock().get_simulation_time()
                                         : 0.0;
    }
    auto get_tick_period() const noexcept -> time_type {
        return 1.0 / simulation_tick_loop.tick_rate;
    }

    auto get_player_ship() const -> ATestSpaceShip const*;
    auto get_player_ship_simulation() noexcept -> ml::test_space_ship::Simulation*;
    auto get_player_ship_simulation() const noexcept -> ml::test_space_ship::Simulation const*;
    void set_player_ship(ATestSpaceShip& new_player_ship);
    void clear_player_ship();
    auto get_lasers() noexcept -> ml::test_lasers::Simulation* {
        return level_simulation_.IsSet() ? level_simulation_->get_lasers() : nullptr;
    }
    auto get_lasers() const noexcept -> ml::test_lasers::Simulation const* {
        return level_simulation_.IsSet() ? level_simulation_->get_lasers() : nullptr;
    }
    auto get_capital_ships() noexcept -> ml::test_capital_ships::Simulation* {
        return level_simulation_.IsSet() ? level_simulation_->get_capital_ships() : nullptr;
    }
    auto get_capital_ships() const noexcept -> ml::test_capital_ships::Simulation const* {
        return level_simulation_.IsSet() ? level_simulation_->get_capital_ships() : nullptr;
    }
    auto get_capital_ship_fighters() noexcept -> ml::test_capital_ship_fighters::Simulation* {
        return level_simulation_.IsSet() ? level_simulation_->get_capital_ship_fighters() : nullptr;
    }
    auto get_capital_ship_fighters() const noexcept
        -> ml::test_capital_ship_fighters::Simulation const* {
        return level_simulation_.IsSet() ? level_simulation_->get_capital_ship_fighters() : nullptr;
    }
    auto get_turrets() noexcept -> ml::test_static_turrets::Simulation* {
        return level_simulation_.IsSet() ? level_simulation_->get_turrets() : nullptr;
    }
    auto get_turrets() const noexcept -> ml::test_static_turrets::Simulation const* {
        return level_simulation_.IsSet() ? level_simulation_->get_turrets() : nullptr;
    }
    auto get_spinners() noexcept -> ml::test_tube_spinners::Simulation* {
        return level_simulation_.IsSet() ? level_simulation_->get_spinners() : nullptr;
    }
    auto get_spinners() const noexcept -> ml::test_tube_spinners::Simulation const* {
        return level_simulation_.IsSet() ? level_simulation_->get_spinners() : nullptr;
    }

    auto get_entity_registry() noexcept -> FTestEntityRegistry& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_entity_registry();
    }
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_entity_registry();
    }
    auto get_level_telemetry_manager() const noexcept -> FLevelTelemetryManager const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_level_telemetry_manager();
    }
    auto get_entity_type(FRegistryEntityHandle const handle) const -> ETestEntityType {
        return get_entity_registry().get_entity_type(handle);
    }
    auto get_spatial_query_manager() noexcept -> ml::FSpatialQueryManager& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_spatial_query_manager();
    }
    auto get_spatial_query_manager() const noexcept -> ml::FSpatialQueryManager const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_spatial_query_manager();
    }
    auto get_mission_manager() noexcept -> FTestMissionManager& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_mission_manager();
    }
    auto get_mission_manager() const noexcept -> FTestMissionManager const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_mission_manager();
    }
    auto get_hud_manager() noexcept -> FHUDManager& { return hud_manager; }
    auto get_hud_manager() const noexcept -> FHUDManager const& { return hud_manager; }
    auto get_hud_update_frequencies() const noexcept -> FTestBatchGameUiUpdateFrequencies const& {
        return hud_update_frequencies;
    }
    auto get_hud_tick_loop() const noexcept { return hud_tick_loop; }

    void set_end_tick_test_hook(FOrchestratorEndTickTestHook hook);
    void clear_end_tick_test_hook();

    void prepare_level();

    static FOnProxyEntitiesBound on_proxy_entities_bound;
    FOnOrchestratorReset on_reset;
    FOnTestMissionCompleted on_mission_completed;
    auto get_mission_definition() -> FLevelMissionDefinition& { return mission_definition; }
    auto get_level_simulation() -> FLevelSimulation* {
        return level_simulation_.IsSet() ? &level_simulation_.GetValue() : nullptr;
    }
    auto get_level_simulation() const -> FLevelSimulation const* {
        return level_simulation_.IsSet() ? &level_simulation_.GetValue() : nullptr;
    }
    auto get_world_collision() -> ml::ioj::FLevelCollisionHost& { return world_collision_; }
    auto get_world_collision() const -> ml::ioj::FLevelCollisionHost const& {
        return world_collision_;
    }
    auto add_static_geometry(UPrimitiveComponent& component) -> bool;
    auto get_presentation_resources() const -> FLevelPresentationResources {
        return make_presentation_resources();
    }
  protected:
    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type end_play_reason) override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Sandbox|Assets")
    void apply_level_config();

    UFUNCTION(CallInEditor, Category = "Sandbox")
    void prepare_level_button();
#endif
  private:
    void begin_play();
    void load_authored_level();
    auto should_initialise_in_begin_play() const noexcept -> bool;
    void validate_proxy_handles();
    void initialise_simulation();
    void process_mission_result();
    auto make_presentation_resources() const -> FLevelPresentationResources;
    void bind_capital_ship_proxy_targets(FProxyEntityMap const& proxy_entities);
    void bind_and_destroy_proxies();
    void start_visual_logging();
    void stop_visual_logging();
    void refresh_collision_grid_visualization();
    void update_collision_bounds_visualization();

    FOrchestratorEndTickTestHook end_tick_test_hook;

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FFixedTickLoop simulation_tick_loop{};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    EOrchestratorStartMode start_mode{EOrchestratorStartMode::Automatic};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Presentation")
    bool presentation_enabled{true};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Visual Logger")
    bool enable_visual_logging{false};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Collision")
    TObjectPtr<UCollisionGridVisualizationComponent> collision_grid_visualization{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Collision|Visualization")
    bool show_collision_bounds{false};
    UPROPERTY(EditAnywhere,
              Category = "Sandbox|Collision|Visualization",
              meta = (ClampMin = "0.0", Units = "cm", EditCondition = "show_collision_bounds"))
    float collision_bounds_max_draw_distance{200000.f};

    FFixedTickLoop hud_tick_loop{};

    FHUDManager hud_manager;
    TOptional<FLevelSimulation> level_simulation_;
    TOptional<ml::FLevelDefinition> level_definition_;
    ml::ioj::FLevelCollisionHost world_collision_;

    UPROPERTY(EditAnywhere, Category = "Sandbox|UI", meta = (ShowOnlyInnerProperties))
    FTestBatchGameUiUpdateFrequencies hud_update_frequencies{};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FLevelMissionDefinition mission_definition;
    UPROPERTY(EditAnywhere, Category = "Sandbox|Presentation")
    FLevelPresentationSettings presentation_settings;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<USandboxISMCComponent> laser_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<UInstancedStaticMeshComponent> capital_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<UInstancedStaticMeshComponent> fighter_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<UInstancedStaticMeshComponent> turret_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<UInstancedStaticMeshComponent> spinner_instances_;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool log_ticks{false};
#endif
};
