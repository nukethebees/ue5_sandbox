#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Sandbox/ui/main_menu/MainMenu2Widget.h"

#include "MainMenu2PlayerController.generated.h"

class ACameraActor;

UCLASS()
class SANDBOX_API AMainMenu2PlayerController : public APlayerController {
    GENERATED_BODY()
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UMainMenu2Widget> main_menu_widget_class;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    ACameraActor* camera_actor;
  private:
    UPROPERTY()
    UMainMenu2Widget* main_menu_widget_instance;
    void spawn_menu_widget();
};
