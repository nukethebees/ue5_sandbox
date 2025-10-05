#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

#include "Delegates/DelegateInstancesImpl.h"
#include "Engine/AssetManager.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

#include "Sandbox/data_assets/BulletDataAsset.h"

#include "Sandbox/macros/null_checks.hpp"

void UBulletSparkEffectSubsystem::add_impact(FSparkEffectTransform&& impact) {
    constexpr auto logger{NestedLogger<"add_impact">()};

    logger.log_verbose(
        TEXT("Adding impact to %s / %s"), *impact.location.ToString(), *impact.rotation.ToString());

    switch (queue.enqueue(std::move(impact))) {
        using enum ml::ELockFreeMPSCQueueEnqueueResult;
        case Success: {
            break;
        }
        case Full: {
            logger.log_warning(TEXT("Queue is full."));
            return;
        }
        case Uninitialised: {
            logger.log_error(TEXT("Queue is uninitialised."));
            return;
        }
        default: {
            break;
        }
    }
}

void UBulletSparkEffectSubsystem::consume_impacts(std::span<FSparkEffectTransform> impacts) {
    constexpr auto logger{NestedLogger<"consume_impacts">()};

    RETURN_IF_NULLPTR(bullet_data);
    RETURN_IF_NULLPTR(bullet_data->ndc_asset);

    auto const n{static_cast<int32>(impacts.size())};
    if (n == 0) {
        return;
    }

    logger.log_verbose(TEXT("Writing %d impacts."), n);

    auto* writer{create_data_channel_writer(*bullet_data->ndc_asset, n)};

    static auto const position_label{FName("position")};
    static auto const rotation_label{FName("rotation")};
    for (int32 i{0}; i < n; ++i) {
        writer->WritePosition(position_label, i, impacts[i].location);
        writer->WriteVector(rotation_label, i, impacts[i].rotation);
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
    constexpr auto logger{NestedLogger<"Initialize">()};

    constexpr std::size_t n_queue_elements{2000};
    switch (queue.init(n_queue_elements)) {
        using enum ml::ELockFreeMPSCQueueInitResult;
        case Success: {
            break;
        }
        case AlreadyInitialised: {
            logger.log_error(TEXT("Queue initialised twice."));
            break;
        }
        default: {
            logger.log_error(TEXT("Unhandled ELockFreeMPSCQueueInitResult state."));
            break;
        }
    }
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
    static FPrimaryAssetId const primary_asset_id{TEXT("Bullet"), TEXT("Standard")};
    auto& asset_manager{UAssetManager::Get()};

    INIT_PTR_OR_RETURN_VALUE(
        loaded_obj, asset_manager.GetPrimaryAssetObject(primary_asset_id), false);
    INIT_PTR_OR_RETURN_VALUE(loaded_data, Cast<UBulletDataAsset>(loaded_obj), false);
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

    queue.swap_and_visit([this](std::span<FSparkEffectTransform> impacts) {
        if (impacts.empty()) {
            return;
        }

        consume_impacts(impacts);
    });
}

TStatId UBulletSparkEffectSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UBulletSparkEffectSubsystem, STATGROUP_Tickables);
}
