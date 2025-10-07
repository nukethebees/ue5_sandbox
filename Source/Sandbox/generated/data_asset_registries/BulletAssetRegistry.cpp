#include "Sandbox/generated/data_asset_registries/BulletAssetRegistry.h"

#include "Engine/StreamableManager.h"
#include "UObject/ConstructorHelpers.h"

namespace ml {

UBulletDataAsset* BulletAssetRegistry::get_bullet() {
    static UBulletDataAsset* cached_asset{nullptr};
    if (!cached_asset) {
        cached_asset =
            LoadObject<UBulletDataAsset>(nullptr, TEXT("/Game/DataAssets/Bullet.Bullet"));
        if (!cached_asset) {
            UE_LOG(LogTemp, Error, TEXT("Failed to load asset: /Game/DataAssets/Bullet.Bullet"));
        }
    }
    return cached_asset;
}

} // namespace ml
