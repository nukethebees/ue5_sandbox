#pragma once

#include <atomic>
#include <cstddef>

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "NiagaraDataChannelPublic.h"
#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/containers/LockFreeMPSCQueueSoA.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

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
    , public ml::LogMsgMixin<"UBulletSparkEffectSubsystem"> {
    GENERATED_BODY()
  public:
    static constexpr std::size_t n_queue_elements{10000};

    template <typename... Args>
    void add_impact(Args&&... args);

    virtual TStatId GetStatId() const override;
  protected:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& world) override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return false; }
  private:
    [[nodiscard]] bool initialise_asset_data();
    void on_end_frame();
    void consume_impacts(FSparkEffectView const& impacts);
    UNiagaraDataChannelWriter* create_data_channel_writer(UNiagaraDataChannelAsset& asset, int32 n);

    UPROPERTY()
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    UPROPERTY()
    FNiagaraDataChannelSearchParameters search_parameters{};

    ml::LockFreeMPSCQueueSoA<FSparkEffectView, FVector, FVector> queue;

    std::atomic<std::size_t> success_count{0};
    std::atomic<std::size_t> full_count{0};
    std::atomic<std::size_t> uninitialised_count{0};
};

template <typename... Args>
void UBulletSparkEffectSubsystem::add_impact(Args&&... args) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UBulletSparkEffectSubsystem::add_impact"))

    switch (queue.enqueue(std::forward<Args>(args)...)) {
        using enum ml::ELockFreeMPSCQueueEnqueueResult;
        case Success: {
            ++success_count;
            break;
        }
        case Full: {
            ++full_count;
            return;
        }
        case Uninitialised: {
            ++uninitialised_count;
            return;
        }
        default: {
            break;
        }
    }
}
