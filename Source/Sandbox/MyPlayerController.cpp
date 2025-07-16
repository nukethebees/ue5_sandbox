// Fill out your copyright notice in the Description page of Project Settings.

#include "MyPlayerController.h"

void AMyPlayerController::print_msg(FString const& msg) {
    auto const fmsg{FString::Printf(TEXT("%s: %s"), *this->GetName(), *msg)};
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, fmsg);
}

void AMyPlayerController::BeginPlay() {
    Super::BeginPlay();

    bEnableClickEvents = true;
    bEnableMouseOverEvents = false;

    if (auto* local_player{GetLocalPlayer()}) {
        using SS = UEnhancedInputLocalPlayerSubsystem;
        if (auto* subsystem{ULocalPlayer::GetSubsystem<SS>(local_player)}) {
            subsystem->AddMappingContext(default_mapping_context.LoadSynchronous(), 0);
        }
    }
}
void AMyPlayerController::OnPossess(APawn* InPawn) {
    Super::OnPossess(InPawn);

    controlled_character = Cast<AMyCharacter>(InPawn);
    if (controlled_character) {
        controlled_character->my_player_controller = this;
    }
}

void AMyPlayerController::SetupInputComponent() {
    Super::SetupInputComponent();

    if (auto* eic = Cast<UEnhancedInputComponent>(InputComponent)) {
        eic->BindAction(look_action.LoadSynchronous(),
                        ETriggerEvent::Triggered,
                        this,
                        &AMyPlayerController::look);
        eic->BindAction(toggle_mouse_action.LoadSynchronous(),
                        ETriggerEvent::Started,
                        this,
                        &AMyPlayerController::toggle_mouse);
    } else {
        print_msg(TEXT("Didn't get EIC."));
    }
}

void AMyPlayerController::look(FInputActionValue const& value) {
    if (controlled_character) {
        controlled_character->look(value);
    }
}
void AMyPlayerController::toggle_mouse(FInputActionValue const& value) {
    auto const mouse_value{value.Get<bool>()};
    print_msg(TEXT("Toggling mouse."));
    bShowMouseCursor = !bShowMouseCursor;
}
