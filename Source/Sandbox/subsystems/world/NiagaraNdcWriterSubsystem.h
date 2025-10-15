#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataChannelPublic.h"

#include "Sandbox/generated/strong_typedefs/NdcWriterIndex.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "NiagaraNdcWriterSubsystem.generated.h"

class UNiagaraDataChannelAsset;
class UNiagaraDataChannelWriter;

// Subsystem for writing to niagara data channels
UCLASS()
class SANDBOX_API UNiagaraNdcWriterSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UNiagaraNdcWriterSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    using NdcAsset = UNiagaraDataChannelAsset;
    using NdcWriter = UNiagaraDataChannelWriter;

    UNiagaraNdcWriterSubsystem() = default;

    auto register_asset(NdcAsset& asset) -> FNdcWriterIndex;
    auto get_asset(FNdcWriterIndex index) -> NdcAsset*;
  protected:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& world) override;
  private:
    void flush_ndc_writes();
    auto create_data_channel_writer(NdcAsset& asset, int32 n) -> NdcWriter*;

    TArray<TWeakObjectPtr<NdcAsset>> assets_;
    TMap<FName, FNdcWriterIndex> asset_lookup_;
    UPROPERTY()
    FNiagaraDataChannelSearchParameters search_parameters{};
};
