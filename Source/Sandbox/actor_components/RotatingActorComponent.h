#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/data/RotationType.h"
#include "RotatingActorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API URotatingActorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    URotatingActorComponent();
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float speed{50.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    ERotationType rotation_type{ERotationType::STATIC};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    bool destroy_component_after_static_registration{true};
  private:
    void unregister_from_subsystem();
};
