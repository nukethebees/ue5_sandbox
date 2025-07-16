// Fill out your copyright notice in the Description page of Project Settings.

#include "MyPlayerController.h"

void AMyPlayerController::print_msg(FString const& msg) {
    auto const fmsg{FString::Printf(TEXT("%s: %s"), *this->GetName(), *msg)};
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, fmsg);
}

void AMyPlayerController::BeginPlay() {
    Super::BeginPlay();

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
        print_msg(TEXT("Got EIC."));
        eic->BindAction(move_action.LoadSynchronous(),
                        ETriggerEvent::Triggered,
                        this,
                        &AMyPlayerController::move);
    } else {
        print_msg(TEXT("Didn't get EIC."));
    }
}

void AMyPlayerController::move(FInputActionValue const& value) {
    print_msg(TEXT("move"));
}
void AMyPlayerController::look(FInputActionValue const& value) {
    print_msg(TEXT("look"));
}
