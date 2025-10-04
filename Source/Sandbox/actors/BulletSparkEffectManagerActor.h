#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"

#include "BulletSparkEffectManagerActor.generated.h"

UCLASS()
class SANDBOX_API ABulletSparkEffectManagerActor : public AActor {
    GENERATED_BODY()
  public:
    ABulletSparkEffectManagerActor();

    UNiagaraComponent* get_niagara_component() const { return niagara_component; }
  protected:
    virtual void BeginPlay() override;
  private:
    UPROPERTY(VisibleAnywhere,
              BlueprintReadOnly,
              Category = "Effects",
              meta = (AllowPrivateAccess = "true"))
    UNiagaraComponent* niagara_component{nullptr};
};
