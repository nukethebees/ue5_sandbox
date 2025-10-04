#pragma once

#include <span>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"

#include "BulletSparkEffectManagerActor.generated.h"

struct FSparkEffectTransform;

UCLASS()
class SANDBOX_API ABulletSparkEffectManagerActor : public AActor {
    GENERATED_BODY()
  public:
    ABulletSparkEffectManagerActor();

    void consume_impacts(std::span<FSparkEffectTransform> impacts);

    UNiagaraComponent* get_niagara_component() const { return niagara_component; }
  protected:
    virtual void BeginPlay() override;
  private:
    UPROPERTY(VisibleAnywhere,
              BlueprintReadOnly,
              Category = "Effects",
              meta = (AllowPrivateAccess = "true"))
    UNiagaraComponent* niagara_component{nullptr};

    TArray<FVector> impact_positions;
    TArray<FQuat> impact_rotations;
};
