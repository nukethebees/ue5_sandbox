#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

#include "SandboxCore/collision_settings.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"

#include "TestCapitalShipsConfig.generated.h"

class UStaticMesh;
class UNiagaraSystem;

class AShipLaser;

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
    UPROPERTY(EditAnywhere)
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere)
    UNiagaraSystem* small_death_explosion{nullptr};

    UPROPERTY(EditAnywhere)
    int32 n_small_explosions{6};

    UPROPERTY(EditAnywhere)
    float time_between_explosions{0.1f};

    UPROPERTY(EditAnywhere)
    FVector3f min_small_explosion_range{FVector3f::ZeroVector};

    UPROPERTY(EditAnywhere)
    FVector3f max_small_explosion_range{FVector3f::OneVector};

    UPROPERTY(EditAnywhere)
    UNiagaraSystem* main_death_explosion{nullptr};

    UPROPERTY(EditAnywhere)
    ETestCapitalShipsMainExplosionDelayMode main_explosion_delay_mode{
        ETestCapitalShipsMainExplosionDelayMode::AfterSmallExplosions};

    UPROPERTY(EditAnywhere)
    float large_explosion_delay{0.0f};

    // Collision
    UPROPERTY(EditAnywhere)
    FCollisionSettings collision_settings;

    // Fighter spawning
    UPROPERTY(EditAnywhere)
    float spawn_delay{5.f};

    UPROPERTY(EditAnywhere)
    int32 fighter_spawn_slots{6};

    UPROPERTY(EditAnywhere)
    TArray<FTransform> fighter_spawn_slots_relative_transforms;

    // Health
    UPROPERTY(EditAnywhere)
    int32 max_health{5000};

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere)
    FVector debug_status_text_offset{0.0, 0.0, 500.0};

    // Proxy settings
    UPROPERTY(EditAnywhere)
    float proxy_arrow_size{5.f};
  private:
    void synchronise_data();
};
