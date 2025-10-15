#include "Sandbox/subsystems/world/NiagaraNdcWriterSubsystem.h"

#include "Misc/CoreDelegates.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelPublic.h"

#include "Sandbox/macros/null_checks.hpp"

auto UNiagaraNdcWriterSubsystem::register_asset(NdcAsset& asset) -> FNdcWriterIndex {
    auto const asset_name{asset.GetFName()};
    if (auto i{asset_lookup_.Find(asset_name)}) {
        return *i;
    }

    FNdcWriterIndex i{assets_.Emplace(&asset)};
    asset_lookup_.Add(asset_name, i);

    queues_.Emplace();

    return i;
}
auto UNiagaraNdcWriterSubsystem::get_asset(FNdcWriterIndex index) -> NdcAsset* {
    auto const i{index.get_value()};
    check((i > 0) && (i < assets_.Num()));
    auto& asset{assets_[i]};
    if (asset.IsValid()) {
        return asset.Get();
    }
    return nullptr;
}

void UNiagaraNdcWriterSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
}
void UNiagaraNdcWriterSubsystem::Deinitialize() {
    FCoreDelegates::OnEndFrame.RemoveAll(this);
    Super::Deinitialize();
}
void UNiagaraNdcWriterSubsystem::OnWorldBeginPlay(UWorld& world) {
    Super::OnWorldBeginPlay(world);
    FCoreDelegates::OnEndFrame.AddUObject(this, &UNiagaraNdcWriterSubsystem::flush_ndc_writes);
}

void UNiagaraNdcWriterSubsystem::flush_ndc_writes() {
    auto const n_assets{num_assets()};

    FString const queue_name{TEXT("UNiagaraNdcWriterSubsystem")};
    static auto const position_label{FName("position")};
    static auto const rotation_label{FName("rotation")};

    for (int32 asset_idx{0}; asset_idx < n_assets; ++asset_idx) {
        auto& queue{queues_[asset_idx]};
        auto& asset_wptr{assets_[asset_idx]};
        CONTINUE_IF_FALSE(asset_wptr.IsValid());
        auto& asset{*asset_wptr.Get()};

        auto flushed_result{queue.swap_and_consume()};
        flushed_result.log_results(queue_name);

        if (flushed_result.is_empty()) {
            continue;
        }

        auto const n_effects{static_cast<int32>(flushed_result.success_count)};
        auto* writer{create_data_channel_writer(asset, n_effects)};
        auto view{flushed_result.view};
        for (int32 effect_idx{0}; effect_idx < n_effects; ++effect_idx) {
            writer->WritePosition(position_label, effect_idx, view.locations[effect_idx]);
            writer->WriteVector(rotation_label, effect_idx, view.rotations[effect_idx]);
        }
    }
}
auto UNiagaraNdcWriterSubsystem::create_data_channel_writer(NdcAsset& asset, int32 n)
    -> NdcWriter* {
    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), nullptr);

    constexpr bool visible_to_game{false};
    constexpr bool visible_to_cpu{true};
    constexpr bool visible_to_gpu{true};
    FString debug_source{};

    auto* writer{UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(world,
                                                                       &asset,
                                                                       search_parameters_,
                                                                       n,
                                                                       visible_to_game,
                                                                       visible_to_cpu,
                                                                       visible_to_gpu,
                                                                       debug_source)};
    return writer;
}
