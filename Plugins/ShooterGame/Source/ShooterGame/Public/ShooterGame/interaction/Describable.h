// IDescribable.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Describable.generated.h"

UINTERFACE(BlueprintType)
class SHOOTERGAME_API UDescribable : public UInterface {
    GENERATED_BODY()
};

class SHOOTERGAME_API IDescribable {
    GENERATED_BODY()
  public:
    UFUNCTION()
    virtual FText get_description() const = 0;
};
