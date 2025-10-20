#include "Sandbox/generated/data_asset_registries/BulletAssetRegistry.h"

#include "Engine/StreamableManager.h"
#include "UObject/ConstructorHelpers.h"

void UBulletAssetRegistry::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    get_bullet_ptr = LoadObject<UBulletDataAsset>(nullptr, TEXT("/Game/DataAssets/Bullet.Bullet"));
    if (!get_bullet_ptr) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load asset: /Game/DataAssets/Bullet.Bullet"));
    }

    get_laserbullet_ptr = LoadObject<UBulletDataAsset>(nullptr, TEXT("/Game/DataAssets/LaserBullet.LaserBullet"));
    if (!get_laserbullet_ptr) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load asset: /Game/DataAssets/LaserBullet.LaserBullet"));
    }

}
