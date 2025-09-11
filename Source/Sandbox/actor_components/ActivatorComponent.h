#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/interfaces/Activatable.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "ActivatorComponent.generated.h"

namespace ml {
inline static constexpr wchar_t ActivatorComponentLogTag[]{TEXT("ActivatorComponent")};
}

class AActor;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UActivatorComponent
    : public UActorComponent
    , public ml::LogMsgMixin<ml::ActivatorComponentLogTag> {
    GENERATED_BODY()
  public:
    UActivatorComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    UFUNCTION(BlueprintCallable)
    void trigger_activation(AActor* instigator);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activation")
    TArray<AActor*> linked_actors;
  private:
    TArray<TScriptInterface<IActivatable>> linked_activatables;
};
