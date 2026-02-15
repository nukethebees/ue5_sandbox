#pragma once

#include "Sandbox/players/playable/space_ship/ShipLaserMode.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShipItemCapture.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;

UCLASS()
class AShipItemCapture : public AActor {
    GENERATED_BODY()
  public:
    AShipItemCapture();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Ship")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ship")
    USceneCaptureComponent2D* camera{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ship")
    FRotator rotation_speed{0.f, 0.f, 0.f};
};
