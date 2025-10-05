#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "NiagaraDataChannelPublic.h"
#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "BulletSparkEffectSubsystem.generated.h"

class UNiagaraComponent;
class UNiagaraDataChannelAsset;
class UNiagaraDataChannelWriter;
class UBulletDataAsset;

USTRUCT()
struct FSparkEffectTransform {
    GENERATED_BODY()

    UPROPERTY()
    FVector location;
    UPROPERTY()
    FVector rotation;

    FSparkEffectTransform() = default;
    FSparkEffectTransform(FVector location, FVector rotation)
        : location(location)
        , rotation(rotation) {}
};

UCLASS()
class SANDBOX_API UBulletSparkEffectSubsystem
    : public UTickableWorldSubsystem
    , public ml::LogMsgMixin<"UBulletSparkEffectSubsystem"> {
    GENERATED_BODY()
  public:
    void add_impact(FSparkEffectTransform&& impact);

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
    void consume_impacts(std::span<FSparkEffectTransform> impacts);
    UNiagaraDataChannelWriter* create_data_channel_writer(UNiagaraDataChannelAsset& asset, int32 n);

    UPROPERTY()
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    UPROPERTY()
    FNiagaraDataChannelSearchParameters search_parameters{};

    ml::LockFreeMPSCQueue<FSparkEffectTransform> queue;
};
