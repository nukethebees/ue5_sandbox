#pragma once

#include <span>

#include "CoreMinimal.h"
#include "NiagaraDataChannelPublic.h"

#include "Sandbox/containers/LockFreeMPSCQueueSoA.h"
#include "Sandbox/containers/MonitoredLockFreeMPSCQueue.h"
#include "Sandbox/generated/strong_typedefs/NdcWriterIndex.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"
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
    struct QueueView {
        std::span<FVector> locations;
        std::span<FVector> rotations;

        QueueView(std::span<FVector> locs, std::span<FVector> rots)
            : locations(locs)
            , rotations(rots) {}
    };

    using NdcAsset = UNiagaraDataChannelAsset;
    using NdcWriter = UNiagaraDataChannelWriter;
    using Queue = ml::LockFreeMPSCQueueSoA<QueueView, FVector, FVector>;
    using MonitoredQueue = ml::MonitoredLockFreeMPSCQueue<Queue>;

    static constexpr std::size_t n_queue_elements_default{5000};

    UNiagaraNdcWriterSubsystem() = default;

    auto register_asset(NdcAsset& asset, std::size_t queue_size = n_queue_elements_default)
        -> FNdcWriterIndex;
    auto get_asset(FNdcWriterIndex index) -> NdcAsset*;
    auto num_assets() const { return num_assets_; }
    template <typename... Args>
    void add_payload(FNdcWriterIndex index, Args&&... args);
  protected:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& world) override;
  private:
    void flush_ndc_writes();
    auto create_data_channel_writer(UWorld& world, NdcAsset& asset, int32 n) -> NdcWriter*;
    void update_owning_component(UWorld & world);

    TArray<TWeakObjectPtr<NdcAsset>> assets_;
    TMap<FName, FNdcWriterIndex> asset_lookup_;
    TArray<MonitoredQueue> queues_;
    int32 num_assets_{0};
    FString writer_debug_source;

    UPROPERTY()
    FNiagaraDataChannelSearchParameters search_parameters_{};
};

template <typename... Args>
void UNiagaraNdcWriterSubsystem::add_payload(FNdcWriterIndex index, Args&&... args) {
    auto const i{index.get_value()};
    check(i >= 0);
    check(i < num_assets());
    (void)queues_[i].enqueue(std::forward<Args>(args)...);
}
