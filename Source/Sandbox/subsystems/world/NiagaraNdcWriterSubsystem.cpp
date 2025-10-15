#include "Sandbox/subsystems/world/NiagaraNdcWriterSubsystem.h"

#include "Misc/CoreDelegates.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelPublic.h"

#include "Sandbox/macros/null_checks.hpp"

auto UNiagaraNdcWriterSubsystem::register_asset(NdcAsset& asset) -> FNdcWriterIndex {
    auto const asset_name{asset.GetFName()};
    if (auto i{asset_lookup_.Find(asset_name)}) {
        return *i;
    }

    FNdcWriterIndex i{assets_.Emplace(&asset)};
    asset_lookup_.Add(asset_name, i);

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

void UNiagaraNdcWriterSubsystem::flush_ndc_writes() {}
auto UNiagaraNdcWriterSubsystem::create_data_channel_writer(NdcAsset& asset, int32 n)
    -> NdcWriter* {
    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), nullptr);

    constexpr bool visible_to_game{false};
    constexpr bool visible_to_cpu{true};
    constexpr bool visible_to_gpu{true};
    FString debug_source{};

    auto* writer{UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(world,
                                                                       &asset,
                                                                       search_parameters,
                                                                       n,
                                                                       visible_to_game,
                                                                       visible_to_cpu,
                                                                       visible_to_gpu,
                                                                       debug_source)};
    return writer;
}
