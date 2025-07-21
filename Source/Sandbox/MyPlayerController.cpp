// Fill out your copyright notice in the Description page of Project Settings.

#include "MyPlayerController.h"
#include "TalkingPillar.h"

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

    if (auto* eic{Cast<UEnhancedInputComponent>(InputComponent)}) {
        eic->BindAction(look_action.LoadSynchronous(),
                        ETriggerEvent::Triggered,
                        this,
                        &AMyPlayerController::look);
        eic->BindAction(toggle_mouse_action.LoadSynchronous(),
                        ETriggerEvent::Started,
                        this,
                        &AMyPlayerController::toggle_mouse);
        eic->BindAction(mouse_click_action.LoadSynchronous(),
                        ETriggerEvent::Started,
                        this,
                        &AMyPlayerController::mouse_click);
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
void AMyPlayerController::mouse_click(FInputActionValue const& value) {
    if (!bShowMouseCursor) {
        return;
    }
    print_msg("Shooting a ray.");

    FVector world_location;
    FVector world_direction;
    if (DeprojectMousePositionToWorld(world_location, world_direction)) {
        auto end{world_location + (world_direction * 10000)};
        FHitResult hit_result;
        GetWorld()->LineTraceSingleByChannel(hit_result, world_location, end, ECC_Visibility);

        if (auto* actor_hit{hit_result.GetActor()}) {
            if (actor_hit->GetClass()->ImplementsInterface(UClickable::StaticClass())) {
                auto* interface{Cast<IClickable>(actor_hit)};
                if (interface) {
                    interface->OnClicked();
                }
            }
        }
    }
}
