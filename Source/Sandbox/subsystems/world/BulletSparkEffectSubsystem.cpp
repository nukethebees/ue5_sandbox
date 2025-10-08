#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

#include "Delegates/DelegateInstancesImpl.h"
#include "Engine/AssetManager.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/generated/data_asset_registries/BulletAssetRegistry.h"

#include "Sandbox/macros/null_checks.hpp"

void UBulletSparkEffectSubsystem::consume_impacts(FSparkEffectView const& impacts) {
    constexpr auto logger{NestedLogger<"consume_impacts">()};

    RETURN_IF_NULLPTR(bullet_data);
    RETURN_IF_NULLPTR(bullet_data->ndc_asset);

    auto const n{static_cast<int32>(impacts.locations.size())};
    if (n == 0) {
        return;
    }

    logger.log_verbose(TEXT("Writing %d impacts."), n);

    auto* writer{create_data_channel_writer(*bullet_data->ndc_asset, n)};

    static auto const position_label{FName("position")};
    static auto const rotation_label{FName("rotation")};
    for (int32 i{0}; i < n; ++i) {
        writer->WritePosition(position_label, i, impacts.locations[i]);
        writer->WriteVector(rotation_label, i, impacts.rotations[i]);
    }
}

UNiagaraDataChannelWriter*
    UBulletSparkEffectSubsystem::create_data_channel_writer(UNiagaraDataChannelAsset& asset,
                                                            int32 n) {

    constexpr bool bVisibleToGame{false};
    constexpr bool bVisibleToCPU{true};
    constexpr bool bVisibleToGPU{true};
    FString DebugSource{};

    // This calls init write for us
    auto* writer{UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(GetWorld(),
                                                                       &asset,
                                                                       search_parameters,
                                                                       n,
                                                                       bVisibleToGame,
                                                                       bVisibleToCPU,
                                                                       bVisibleToGPU,
                                                                       DebugSource)};
    return writer;
}

void UBulletSparkEffectSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    (void)queue.logged_init(n_queue_elements, "BulletSparkEffectSubsystem");
}

void UBulletSparkEffectSubsystem::Deinitialize() {
    FCoreDelegates::OnEndFrame.RemoveAll(this);
    Super::Deinitialize();
}
void UBulletSparkEffectSubsystem::OnWorldBeginPlay(UWorld& world) {
    constexpr auto logger{NestedLogger<"OnWorldBeginPlay">()};
    Super::OnWorldBeginPlay(world);

    if (!initialise_asset_data()) {
        logger.log_warning(TEXT("Failed to load the asset data."));
        return;
    }

    FCoreDelegates::OnEndFrame.AddUObject(this, &UBulletSparkEffectSubsystem::on_end_frame);
}

bool UBulletSparkEffectSubsystem::initialise_asset_data() {
    constexpr auto logger{NestedLogger<"initialise_asset_data">()};
    INIT_PTR_OR_RETURN_VALUE(loaded_data, ml::BulletAssetRegistry::get_bullet(), false);
    bullet_data = loaded_data;
    return true;
}

void UBulletSparkEffectSubsystem::Tick(float DeltaTime) {}

void UBulletSparkEffectSubsystem::on_end_frame() {
    if (auto* world{GetWorld()}) {
        if (!world->IsGameWorld()) {
            return;
        }
        if (world->IsPaused()) {
            return;
        }
    }

    queue.swap_and_visit([this](auto const& result) {
        result.log_results(TEXT("UBulletSparkEffectSubsystem"));
        if (result.view.locations.empty()) {
            return;
        }

        consume_impacts(result.view);
    });
}

TStatId UBulletSparkEffectSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UBulletSparkEffectSubsystem, STATGROUP_Tickables);
}
