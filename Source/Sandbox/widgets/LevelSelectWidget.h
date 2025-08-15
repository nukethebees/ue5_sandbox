#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"

#include "LevelSelectWidget.generated.h"

class UMainMenuWidget;

USTRUCT(BlueprintType)
struct FLevelButtonBinding {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName level_name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UButton* button;
};

UCLASS()
class SANDBOX_API ULevelSelectWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    virtual bool Initialize() override;

    UPROPERTY()
    UMainMenuWidget* parent_menu{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Level1;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Level2;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Level3;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Level4;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Level5;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Level6;
  private:
    UFUNCTION()
    void open_level_1();
    UFUNCTION()
    void open_level_2();
    UFUNCTION()
    void open_level_3();
    UFUNCTION()
    void open_level_4();
    UFUNCTION()
    void open_level_5();
    UFUNCTION()
    void open_level_6();
    void open_level_x(int32 number);
};
