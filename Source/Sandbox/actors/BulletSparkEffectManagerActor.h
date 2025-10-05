#pragma once

#include <span>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "BulletSparkEffectManagerActor.generated.h"

class UNiagaraDataChannelAsset;

struct FSparkEffectTransform;

UCLASS()
class SANDBOX_API ABulletSparkEffectManagerActor
    : public AActor
    , public ml::LogMsgMixin<"ABulletSparkEffectManagerActor"> {
    GENERATED_BODY()
  public:
    ABulletSparkEffectManagerActor();

    void consume_impacts(std::span<FSparkEffectTransform> impacts);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
    TObjectPtr<UNiagaraDataChannelAsset> ndc_asset{nullptr};
  protected:
    virtual void BeginPlay() override;
};
