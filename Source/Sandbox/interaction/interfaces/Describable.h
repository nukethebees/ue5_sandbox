// IDescribable.h
#pragma once

#include "CoreMinimal.h"

#include "UObject/Interface.h"

#include "Describable.generated.h"

UINTERFACE(BlueprintType)
class UDescribable : public UInterface {
    GENERATED_BODY()
};

class IDescribable {
    GENERATED_BODY()
  public:
    UFUNCTION()
    virtual FText const& get_description() const = 0;
};
