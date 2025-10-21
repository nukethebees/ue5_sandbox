#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ItemDetailsWidget.generated.h"

UCLASS()
class SANDBOX_API UItemDetailsWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;
};
