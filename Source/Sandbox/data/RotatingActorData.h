#pragma once

#include "CoreMinimal.h"
#include "RotatingActorData.generated.h"

class URotatingActorComponent;

USTRUCT()
struct SANDBOX_API FRotatingActorData {
    GENERATED_BODY()

    URotatingActorComponent* Component;
    TWeakObjectPtr<AActor> Actor;
};
