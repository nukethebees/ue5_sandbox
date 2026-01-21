#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/inventory/interfaces/InventoryItem.h"

#include "Rocket.generated.h"

class UTexture2D;
class UBoxComponent;

UCLASS()
class SANDBOX_API ARocket : public AActor {
    GENERATED_BODY()
  public:
    ARocket();
};
