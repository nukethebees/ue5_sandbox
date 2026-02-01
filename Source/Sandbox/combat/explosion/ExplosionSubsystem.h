#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/combat/explosion/ExplosionConfig.h"
#include "Sandbox/environment/effects/data/generated/NdcWriterIndex.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "ExplosionSubsystem.generated.h"

UCLASS()
class SANDBOX_API UExplosionSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UExplosionSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    void spawn_explosion(FVector location, FRotator rotation, FExplosionConfig const& config);

    virtual void Initialize(FSubsystemCollectionBase& collection) override;
  private:
    void execute_explosion(FVector location,
                           FRotator rotation,
                           FExplosionConfig const& config,
                           FNdcWriterIndex writer_index);
    FVector calculate_impulse(FVector explosion_location,
                              FVector target_location,
                              float target_distance,
                              float explosion_radius,
                              float explosion_force);
};
