#pragma once

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestBatchOrchestrator.generated.h"

class ATestSpaceShip;
class ATestLasers;
class ATestCapitalShips;
class ATestCapitalShipFighters;
class ATestStaticTurrets;
class ATestTubeSpinners;

class ATestEntityRegistry;
class ATestMissionManager;
class ADelayedNiagaraSpawner;

class ATestBatchOrchestrator;

DECLARE_DELEGATE_OneParam(FOrchestratorEndTickTestHook, ATestBatchOrchestrator&);

UENUM(BlueprintType)
enum class EOrchestratorStartMode : uint8 {
    Paused,
    Automatic,
};

UCLASS()
class SANDBOX_API ATestBatchOrchestrator : public AActor {
    GENERATED_BODY()
  public:
    using tick_type = uint64;
    using time_type = double;

    ATestBatchOrchestrator();

    void Tick(float dt) override;
    void tick(time_type const dt);
    void start_simulation();

    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return static_cast<time_type>(completed_ticks) * tick_period;
    }

    auto get_player_ship() const -> auto const* { return player_ship.Get(); }
    auto get_lasers() const -> auto const* { return lasers.Get(); }
    auto get_capital_ships() const -> auto const* { return capital_ships.Get(); }
    auto get_capital_ship_fighters() const -> auto const* { return capital_ship_fighters.Get(); }
    auto get_turrets() const -> auto const* { return turrets.Get(); }
    auto get_spinners() const -> auto const* { return spinners.Get(); }

    auto get_entity_registry() const { return entity_registry; }

    void set_end_tick_test_hook(FOrchestratorEndTickTestHook hook);
    void clear_end_tick_test_hook();
  protected:
    void BeginPlay() override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Sandbox")
    void spawn_missing_actors();
#endif
  private:
    void begin_play();
    void validate_proxy_handles();
    void route_actor_references();

    FOrchestratorEndTickTestHook end_tick_test_hook;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double tick_rate{60.f};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double time_scale{1.f};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    EOrchestratorStartMode start_mode{EOrchestratorStartMode::Automatic};

    time_type tick_period{0.f};
    time_type accumulator{0.f};

    tick_type completed_ticks{0};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> lasers{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestCapitalShips> capital_ships{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestCapitalShipFighters> capital_ship_fighters{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestStaticTurrets> turrets{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestTubeSpinners> spinners{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestMissionManager> mission_manager{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ADelayedNiagaraSpawner> niagara_spawner{nullptr};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ATestSpaceShip> player_ship_class{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ATestLasers> lasers_class{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ATestCapitalShips> capital_ships_class{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ATestCapitalShipFighters> capital_ship_fighters_class{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ATestStaticTurrets> turrets_class{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ATestTubeSpinners> spinners_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ATestEntityRegistry> entity_registry_class{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ATestMissionManager> mission_manager_class{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TSubclassOf<ADelayedNiagaraSpawner> niagara_spawner_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool log_ticks{false};
#endif
};
