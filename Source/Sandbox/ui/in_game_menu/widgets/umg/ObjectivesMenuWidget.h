#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ObjectivesMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UObjectivesMenuWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;
};
