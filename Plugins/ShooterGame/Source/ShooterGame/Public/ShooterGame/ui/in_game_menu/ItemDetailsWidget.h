#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ItemDetailsWidget.generated.h"

UCLASS()
class SHOOTERGAME_API UItemDetailsWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    void NativeConstruct() override;
};
