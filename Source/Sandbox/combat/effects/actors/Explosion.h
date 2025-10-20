#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraDataChannel.h"

#include "Sandbox/environment/effects/data/generated/NdcWriterIndex.h"
#include "Sandbox/health/data/HealthChange.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "Explosion.generated.h"

UCLASS()
class SANDBOX_API AExplosion
    : public AActor
    , public ml::LogMsgMixin<"AExplosion", LogSandboxActor> {
    GENERATED_BODY()
  public:
    AExplosion();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    FHealthChange damage_config{25.0f, EHealthChangeType::Damage};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    float explosion_radius{300.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    float explosion_force{5000.0f};
  protected:
    virtual void BeginPlay() override;
    virtual void Tick(float delta_time) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    TObjectPtr<UNiagaraDataChannelAsset> explosion_channel{};
  private:
    void explode();
    FVector calculate_impulse(FVector target_location, float target_distance);

    FNdcWriterIndex writer_index;
};
