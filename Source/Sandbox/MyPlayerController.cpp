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
void AMyPlayerController::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);

    if (!controlled_character) {
        print_msg(TEXT("No controlled character"));
        return;
    }

    if (bShowMouseCursor) {
        FVector world_location;
        FVector world_direction;
        if (DeprojectMousePositionToWorld(world_location, world_direction)) {
            auto const target_location{world_location + world_direction * 1000.0f};
            controlled_character->aim_torch(target_location);
        }
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

        eic->BindAction(toggle_torch_action.LoadSynchronous(),
                        ETriggerEvent::Started,
                        this,
                        &AMyPlayerController::toggle_torch);

        eic->BindAction(scroll_torch_cone_action.LoadSynchronous(),
                        ETriggerEvent::Triggered,
                        this,
                        &AMyPlayerController::scroll_torch_cone);

        eic->BindAction(warp_to_cursor_action.LoadSynchronous(),
                        ETriggerEvent::Completed,
                        this,
                        &AMyPlayerController::warp_to_cursor);
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

    if (bShowMouseCursor) {
        print_msg(TEXT("Disabling mouse cursor."));

        auto input_mode{FInputModeGameOnly()};
        SetInputMode(input_mode);
        bShowMouseCursor = false;
        controlled_character->reset_torch();
    } else {
        print_msg(TEXT("Enabling mouse cursor."));

        auto input_mode{FInputModeGameAndUI()};
        SetInputMode(input_mode);
        bShowMouseCursor = true;
    }
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
void AMyPlayerController::toggle_torch(FInputActionValue const& value) {
    if (controlled_character) {
        controlled_character->toggle_torch();
    }
}
void AMyPlayerController::scroll_torch_cone(FInputActionValue const& value) {
    if (!controlled_character || !controlled_character->torch_component) {
        print_msg("No char or torch");
    }

    auto const scroll_delta{value.Get<float>()};
    auto* const torch{controlled_character->torch_component};

    static constexpr auto min_cone{5.0f};
    static constexpr auto max_cone{15.0f};

    auto const new_outer{
        FMath::Clamp(torch->OuterConeAngle + scroll_delta * 2.0f, min_cone, max_cone)};
    torch->SetOuterConeAngle(new_outer);

    auto const new_inner{FMath::Clamp(new_outer * 0.7f, 2.0f, new_outer)};
    torch->SetInnerConeAngle(new_inner);
}
void AMyPlayerController::warp_to_cursor(FInputActionValue const& value) {
    if (!controlled_character) {
        print_msg(TEXT("No char to warp"));
        return;
    }
    if (!bShowMouseCursor) {
        return;
    }

    FVector world_location;
    FVector world_direction;

    if (DeprojectMousePositionToWorld(world_location, world_direction)) {
        auto const start{world_location};
        auto const end{start + world_direction * 10000.0f};

        FHitResult hit;
        FCollisionQueryParams params;
        params.AddIgnoredActor(controlled_character);

        if (GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params)) {
            controlled_character->warp_component->warp_to_location(controlled_character,
                                                                   hit.Location);
        }
    }
}
