#pragma once

#include "GameFramework/PlayerController.h"

#include "MainMenuPlayerController.generated.h"

namespace ml::ioj {
class UMainMenuWidget;

UCLASS()
class SPACEGAME_API AMainMenuPlayerController : public APlayerController {
    GENERATED_BODY()
  public:
    AMainMenuPlayerController();
  protected:
    void BeginPlay() override;
  private:
    void select_menu_camera();
    void create_main_menu();

    UPROPERTY()
    TSubclassOf<UMainMenuWidget> main_menu_widget_class{nullptr};

    UPROPERTY(Transient)
    UMainMenuWidget* main_menu_widget_{nullptr};
};
}
