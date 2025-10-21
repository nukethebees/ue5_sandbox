#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MapMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UMapMenuWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;
};
