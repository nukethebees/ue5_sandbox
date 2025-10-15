#pragma once

#include <atomic>
#include <cstddef>

#include "CoreMinimal.h"
#include "NiagaraDataChannelPublic.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/containers/LockFreeMPSCQueueSoA.h"
#include "Sandbox/containers/MonitoredLockFreeMPSCQueue.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "BulletSparkEffectSubsystem.generated.h"

class UNiagaraComponent;
class UNiagaraDataChannelAsset;
class UNiagaraDataChannelWriter;
class UBulletDataAsset;

struct FSparkEffectView {
    std::span<FVector> locations;
    std::span<FVector> rotations;

    FSparkEffectView(std::span<FVector> locs, std::span<FVector> rots)
        : locations(locs)
        , rotations(rots) {}
};

UCLASS()
class SANDBOX_API UBulletSparkEffectSubsystem
    : public UTickableWorldSubsystem
    , public ml::LogMsgMixin<"UBulletSparkEffectSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    using NdcAsset = UNiagaraDataChannelAsset;
    using NdcWriter = UNiagaraDataChannelWriter;

    static constexpr std::size_t n_queue_elements{30000};

    template <typename... Args>
    void add_impact(Args&&... args);

    virtual TStatId GetStatId() const override;
  protected:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& world) override;
    virtual bool IsTickable() const override { return false; }
  private:
    [[nodiscard]] bool initialise_asset_data();
    void on_end_frame();
    void consume_impacts(FSparkEffectView const& impacts);
    NdcWriter* create_data_channel_writer(NdcAsset& asset, int32 n);

    UPROPERTY()
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    UPROPERTY()
    FNiagaraDataChannelSearchParameters search_parameters{};

    // Position, rotation
    ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueueSoA<FSparkEffectView, FVector, FVector>>
        queue;
};

template <typename... Args>
void UBulletSparkEffectSubsystem::add_impact(Args&&... args) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UBulletSparkEffectSubsystem::add_impact"))

    (void)queue.enqueue(std::forward<Args>(args)...);
}
