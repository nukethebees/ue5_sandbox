#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "BulletSparkEffectSubsystem.generated.h"

class UNiagaraComponent;
class ABulletSparkEffectManagerActor;

struct FSparkEffectTransform {
    FVector location;
    FRotator rotation;
};

UCLASS()
class SANDBOX_API UBulletSparkEffectSubsystem
    : public UTickableWorldSubsystem
    , public ml::LogMsgMixin<"UBulletSparkEffectSubsystem"> {
    GENERATED_BODY()
  public:
    void add_impact(FVector const& location, FRotator const& rotation);
    void register_actor(ABulletSparkEffectManagerActor* actor);

    virtual TStatId GetStatId() const override;
  protected:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return false; }
  private:
    void on_end_frame();

    ABulletSparkEffectManagerActor* manager_actor{nullptr};
    ml::LockFreeMPSCQueue<FSparkEffectTransform> queue;
};
