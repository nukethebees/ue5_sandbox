#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "CoreMinimal.h"

#include "HealthWidget.generated.h"

UCLASS()
class SANDBOX_API UHealthWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UFUNCTION()
    void set_health_percent(float percent);
  protected:
    UPROPERTY(meta = (BindWidget))
    class UProgressBar* health_bar;
};
