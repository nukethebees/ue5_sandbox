#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "SandboxGameShared/combat/explosion/ExplosionConfig.h"
#include "SandboxGameShared/logging/SandboxGameSharedLogCategories.h"
#include "SandboxGameShared/environment/effects/NdcWriterIndex.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"

#include "ExplosionSubsystem.generated.h"

UCLASS()
class SANDBOXGAMESHARED_API UExplosionSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UExplosionSubsystem", LogSandboxGameShared> {
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
