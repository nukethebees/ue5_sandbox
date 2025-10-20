#include "Sandbox/environment/effects/subsystems/NiagaraNdcWriterSubsystem.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelPublic.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

auto UNiagaraNdcWriterSubsystem::register_asset(NdcAsset& asset, std::size_t queue_size)
    -> FNdcWriterIndex {
    auto const asset_name{asset.GetFName()};
    if (auto i{asset_lookup_.Find(asset_name)}) {
        return *i;
    }

    FNdcWriterIndex i{assets_.Emplace(&asset)};
    asset_lookup_.Add(asset_name, i);

    auto& queue{queues_.Emplace_GetRef()};
    (void)queue.logged_init(queue_size, "UNiagaraNdcWriterSubsystem");

    // Defer incrementing the count until we're fully initialised
    num_assets_ = i.get_value() + 1;

    log_display(TEXT("Registered asset: %s"), *asset_name.ToString());

    return i;
}
auto UNiagaraNdcWriterSubsystem::get_asset(FNdcWriterIndex index) -> NdcAsset* {
    auto const i{index.get_value()};
    check((i > 0) && (i < num_assets()));
    auto& asset{assets_[i]};
    if (asset.IsValid()) {
        return asset.Get();
    }
    return nullptr;
}

void UNiagaraNdcWriterSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    log_display(TEXT("Initialized"));
    writer_debug_source = TEXT("UNiagaraNdcWriterSubsystem");
}
void UNiagaraNdcWriterSubsystem::Deinitialize() {
    FCoreDelegates::OnEndFrame.RemoveAll(this);
    Super::Deinitialize();
}
void UNiagaraNdcWriterSubsystem::OnWorldBeginPlay(UWorld& world) {
    Super::OnWorldBeginPlay(world);
    FCoreDelegates::OnEndFrame.AddUObject(this, &UNiagaraNdcWriterSubsystem::flush_ndc_writes);
    update_owning_component(world);
}

void UNiagaraNdcWriterSubsystem::flush_ndc_writes() {
    auto const n_assets{num_assets()};

    TRY_INIT_PTR(world, GetWorld());
    if (!search_parameters_.OwningComponent) {
        update_owning_component(*world);
    }

    static auto const position_label{FName("spawn_position")};
    static auto const rotation_label{FName("spawn_rotation")};

    for (int32 asset_idx{0}; asset_idx < n_assets; ++asset_idx) {
        auto& queue{queues_[asset_idx]};
        auto& asset_wptr{assets_[asset_idx]};
        CONTINUE_IF_FALSE(asset_wptr.IsValid());
        auto& asset{*asset_wptr.Get()};

        auto flushed_result{queue.swap_and_consume()};
        flushed_result.log_results(writer_debug_source);

        if (flushed_result.is_empty()) {
            continue;
        }

        auto const n_effects{static_cast<int32>(flushed_result.success_count)};
        auto view{flushed_result.view};
        auto* writer{create_data_channel_writer(*world, asset, n_effects)};
        for (int32 effect_idx{0}; effect_idx < n_effects; ++effect_idx) {
            writer->WritePosition(position_label, effect_idx, view.locations[effect_idx]);
            writer->WriteVector(rotation_label, effect_idx, view.rotations[effect_idx]);
        }
    }
}
auto UNiagaraNdcWriterSubsystem::create_data_channel_writer(UWorld& world, NdcAsset& asset, int32 n)
    -> NdcWriter* {
    constexpr bool visible_to_game{false};
    constexpr bool visible_to_cpu{true};
    constexpr bool visible_to_gpu{true};

    auto* writer{UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(&world,
                                                                       &asset,
                                                                       search_parameters_,
                                                                       n,
                                                                       visible_to_game,
                                                                       visible_to_cpu,
                                                                       visible_to_gpu,
                                                                       writer_debug_source)};
    return writer;
}
void UNiagaraNdcWriterSubsystem::update_owning_component(UWorld& world) {
    // This is important
    // Without this the emitter will be spawned at the origin and particles
    // will be culled unless you were looking at the origin
    if (auto* character{UGameplayStatics::GetPlayerCharacter(&world, 0)}) {
        search_parameters_.OwningComponent = character->GetRootComponent();
    }
}
