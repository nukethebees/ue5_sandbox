#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Sandbox/widgets/MainMenu2Widget.h"

#include "MainMenu2PlayerController.generated.h"

UCLASS()
class SANDBOX_API AMainMenu2PlayerController : public APlayerController {
    GENERATED_BODY()
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UMainMenu2Widget> main_menu_widget_class;
  private:
    UPROPERTY()
    UMainMenu2Widget* main_menu_widget_instance;

    void spawn_menu_widget();
};
