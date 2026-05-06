#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformFieldFly0.generated.h"

class UStaticMeshComponent;

class ATestUniformField;

UCLASS()
class ATestUniformFieldFly0 : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformFieldFly0();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    void on_field_post_construction(ATestUniformField& field_);

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    TObjectPtr<ATestUniformField> field{nullptr};

    UPROPERTY(EditInstanceOnly, Category = "Ship")
    FVector destination{FVector::ZeroVector};
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    bool show_destination{true};
};
