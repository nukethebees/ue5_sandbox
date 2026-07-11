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

UCLASS()
class ATestBatchOrchestrator : public AActor {
    GENERATED_BODY()
  public:
    ATestBatchOrchestrator();

    void Tick(float dt) override;
    void tick(float const dt);

    auto get_tick_count() const noexcept { return tick_counter; }

    auto get_lasers() const -> auto const* { return lasers.Get(); }
    auto get_capital_ships() const -> auto const* { return capital_ships.Get(); }
    auto get_capital_ship_fighters() const -> auto const* { return capital_ship_fighters.Get(); }
    auto get_turrets() const -> auto const* { return turrets.Get(); }
    auto get_spinners() const -> auto const* { return spinners.Get(); }

    auto get_entity_registry() const { return entity_registry; }
  protected:
    void BeginPlay() override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Sandbox")
    void spawn_missing_actors();
#endif
  private:
    void validate_proxy_handles();
    void route_actor_references();

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

    UPROPERTY(Transient)
    uint64 tick_counter{0};

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
#endif
};
