#include "SpaceGame/ui/main_menu/MainMenuGameMode.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include "SpaceGame/ships/player/SpaceGamePlayerController.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"

#include <Engine/GameInstance.h>
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

void AMainMenuGameMode::StartPlay() {
    Super::StartPlay();

    auto* const game_instance{GetGameInstance()};
    auto* const game{IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>()
                                            : nullptr};
    if (!IsValid(game)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("AMainMenuGameMode::StartPlay: Game subsystem is unavailable; menu "
                    "ambience will not play."));
        return;
    }
    game->start_menu_ambience();
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

void AMainMenuGameMode::EndPlay(EEndPlayReason::Type const reason) {
    auto* const game_instance{GetGameInstance()};
    auto* const game{IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>()
                                            : nullptr};
    if (IsValid(game)) {
        game->stop_menu_ambience();
    }

    Super::EndPlay(reason);
}
}
