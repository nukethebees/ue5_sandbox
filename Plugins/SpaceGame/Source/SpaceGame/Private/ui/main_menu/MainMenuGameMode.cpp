#include "SpaceGame/ui/main_menu/MainMenuGameMode.h"

#include "SpaceGame/ui/main_menu/MainMenuPlayerController.h"

namespace ml::ioj {
AMainMenuGameMode::AMainMenuGameMode() {
    PlayerControllerClass = AMainMenuPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
}
}
