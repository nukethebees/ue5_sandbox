#include "SpaceGame/ui/main_menu/MainMenuGameMode.h"

#include "SpaceGame/ships/player/SpaceGamePlayerController.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <UObject/ConstructorHelpers.h>

namespace ml::ioj {
AMainMenuGameMode::AMainMenuGameMode() {
    static ConstructorHelpers::FClassFinder<ASpaceGamePlayerController> controller_class{
        TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController")};
    PlayerControllerClass = ASpaceGamePlayerController::StaticClass();
    if (controller_class.Succeeded()) {
        PlayerControllerClass = controller_class.Class;
    }
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
}

void AMainMenuGameMode::PostLogin(APlayerController* const new_player) {
    Super::PostLogin(new_player);

    auto* const controller{Cast<ASpaceGamePlayerController>(new_player)};
    if (!IsValid(controller)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("AMainMenuGameMode::PostLogin: Player controller is not a space-game "
                    "controller."));
        return;
    }
    controller->show_main_menu();
}
}
