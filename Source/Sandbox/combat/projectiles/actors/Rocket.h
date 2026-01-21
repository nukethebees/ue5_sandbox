#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/interaction/interfaces/Describable.h"

#include "Rocket.generated.h"

class UStaticMeshComponent;

UCLASS()
class SANDBOX_API ARocket
    : public AActor
    , public IDescribable {
    GENERATED_BODY()
  public:
    ARocket();

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Rocket"))};
        return desc;
    }
  protected:
    UPROPERTY(EditAnywhere, Category = "Rocket")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Rocket")
    float speed{500.f};
};
