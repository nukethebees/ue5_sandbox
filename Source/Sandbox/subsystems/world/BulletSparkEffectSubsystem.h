#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "BulletSparkEffectSubsystem.generated.h"

class UNiagaraComponent;
class ABulletSparkEffectManagerActor;

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
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Tick(float DeltaTime) override;
  private:
    TArray<FVector> impact_locations;
    TArray<FRotator> impact_rotations;
    ABulletSparkEffectManagerActor* manager_actor{nullptr};
    ml::LockFreeMPSCQueue<FVector> queue;
};
