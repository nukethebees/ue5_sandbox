#pragma once

#include "SpaceGamePresentation/entities/TestTeam.h"
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestCapitalShipProxy.generated.h"

class UStaticMeshComponent;
class UArrowComponent;

UCLASS()
class ATestCapitalShipProxy : public AActor {
    GENERATED_BODY()
  public:
    ATestCapitalShipProxy();

    SPACEGAME_API void set_level_config_asset(USpaceGameLevelConfig* new_config) noexcept;

    auto get_team() const noexcept { return team; }

    void set_team(ETestTeam const new_team) noexcept { team = new_team; }

#if WITH_EDITOR
    void Tick(float delta_seconds) override;
    auto ShouldTickIfViewportsOnly() const -> bool override { return true; }
    void PostRegisterAllComponents() override;

    UFUNCTION(CallInEditor, Category = "Ship")
    void apply_asset_configuration();
    UFUNCTION(CallInEditor, Category = "Ship")
    void apply_asset_configuration_to_all_instances();
    UFUNCTION(CallInEditor, Category = "Ship")
    void save_configuration_to_asset();
    UFUNCTION(CallInEditor, Category = "Ship")
    void diagnose_fighter_spawn_points();

#endif

  protected:
    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(VisibleInstanceOnly, Category = "Ship|Configuration")
    TObjectPtr<USpaceGameLevelConfig> level_config_asset{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TArray<TObjectPtr<UArrowComponent>> fighter_spawn_slots;

    UPROPERTY(EditAnywhere, Category = "Level Authoring")
    ETestTeam team{ETestTeam::White};

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleInstanceOnly, Transient, Category = "Ship|Configuration")
    FString spawn_configuration_status;

    UPROPERTY(EditAnywhere, Category = "Ship|Spawn Preview")
    bool show_fighter_spawn_preview{true};

#endif

#if WITH_EDITOR
    void apply_spawn_configuration(USpaceGameLevelConfig& config);
    void update_spawn_configuration_status();
    void draw_fighter_spawn_preview();
    FString spawn_preview_error;
#endif

  public:
    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "0.0", Units = "s"))
    double spawn_time_seconds{};

    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "1"))
    TOptional<int32> health;

    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "0.0"))
    TOptional<float> initial_spawn_delay;

    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "0.0"))
    TOptional<float> spawn_cooldown;
};
