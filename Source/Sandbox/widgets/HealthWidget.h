#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Sandbox/data/HealthData.h"

#include "HealthWidget.generated.h"

UCLASS()
class SANDBOX_API UHealthWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UFUNCTION()
    void set_health(FHealthData health_data);
  protected:
    UPROPERTY(meta = (BindWidget))
    class UProgressBar* health_bar;
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* health_text;
};
