#pragma once

#include "CoreMinimal.h"

#include "Subsystems/GameInstanceSubsystem.h"
#include "Sandbox/data_assets/BulletDataAsset.h"

#include "BulletAssetRegistry.generated.h"

/**
 * GENERATED CODE - DO NOT EDIT MANUALLY
 * Auto-generated subsystem for accessing data assets.
 * Assets are loaded once on initialization and held by UPROPERTY to prevent GC.
 * Regenerate using the SandboxEditor toolbar button.
 *
 * Usage: UGameInstance::GetSubsystem<UBulletAssetRegistry>(GameInstance)->get_bullet()
 */
UCLASS()
class UBulletAssetRegistry : public UGameInstanceSubsystem {
    GENERATED_BODY()

  public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UBulletDataAsset* get_bullet() const { return get_bullet_ptr; }
    UBulletDataAsset* get_laserbullet() const { return get_laserbullet_ptr; }

  private:
    UPROPERTY()
    TObjectPtr<UBulletDataAsset> get_bullet_ptr{nullptr};
    UPROPERTY()
    TObjectPtr<UBulletDataAsset> get_laserbullet_ptr{nullptr};
};
