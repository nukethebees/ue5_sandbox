#pragma once

#include "SpaceGame/support/DrawDebugConfig.h"

#include "SandboxCoreEngine/collision_settings.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"

#include "TestCapitalShipsConfig.generated.h"

class UStaticMesh;
class UNiagaraSystem;
class UMaterialInterface;

class AShipLaser;
class UTestTeamVisualData;
class USandboxVisualLoggerStyle;

UENUM()
enum class ETestCapitalShipsMainExplosionDelayMode : uint8 {
    AfterSmallExplosions,
    Absolute,
};

UCLASS(BlueprintType)
class UTestCapitalShipsConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UTestCapitalShipsConfig() = default;

#if WITH_EDITOR
    void PostEditChangeProperty(FPropertyChangedEvent& event) override;
    void PostLoad() override;
#endif

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UMaterialInterface> material;

    UPROPERTY(EditAnywhere, Category = "Visuals")
    UNiagaraSystem* small_death_explosion{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    int32 n_small_explosions{6};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float time_between_explosions{0.1f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FVector3f min_small_explosion_range{FVector3f::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FVector3f max_small_explosion_range{FVector3f::OneVector};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    UNiagaraSystem* main_death_explosion{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    ETestCapitalShipsMainExplosionDelayMode main_explosion_delay_mode{
        ETestCapitalShipsMainExplosionDelayMode::AfterSmallExplosions};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float large_explosion_delay{0.0f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    // Collision
    UPROPERTY(EditAnywhere, Category = "Collision")
    FCollisionSettings collision_settings;

    // Fighter spawning
    UPROPERTY(EditAnywhere, Category = "Fighters")
    float spawn_delay{5.f};

    UPROPERTY(EditAnywhere, Category = "Fighters")
    int32 fighter_spawn_slots{6};

    UPROPERTY(EditAnywhere, Category = "Fighters")
    TArray<FTransform> fighter_spawn_slots_relative_transforms;

    // Health
    UPROPERTY(EditAnywhere, Category = "Health")
    int32 max_health{5000};

    // Debugging
    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Debug")
    FVector debug_status_text_offset{0.0, 0.0, 500.0};

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    TObjectPtr<USandboxVisualLoggerStyle> visual_logger_style{nullptr};

    // Proxy settings
    UPROPERTY(EditAnywhere, Category = "Proxy")
    float proxy_arrow_size{5.f};
  private:
    void synchronise_data();
};
