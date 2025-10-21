#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "PowersMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UPowersMenuWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;
};
