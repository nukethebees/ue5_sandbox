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

    void set_new_destination();
    void move_to_destination();
    bool at_target() const;

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    TObjectPtr<ATestUniformField> field{nullptr};

    // Navigation
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    FVector destination{FVector::ZeroVector};
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    bool show_destination{true};
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    float acceptance_radius{1000.f};
    // Movement
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    float speed{1000.f};
};
