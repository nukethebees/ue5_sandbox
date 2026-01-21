#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Rocket.generated.h"

class UStaticMeshComponent;

UCLASS()
class SANDBOX_API ARocket : public AActor {
    GENERATED_BODY()
  public:
    ARocket();
  protected:
    UPROPERTY(EditAnywhere, Category = "Rocket")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Rocket")
    float speed{500.f};
};
